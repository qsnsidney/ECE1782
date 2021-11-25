#include "simple_engine.cuh"
#include "core/timer.h"

#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <assert.h>
#include <iostream>

#include "core/physics.hpp"
#include "core/serde.h"
#include "helper.h"
#include "data_t.h"
#include "constant.h"
#include "basic_kernel.h"

namespace TUS
{
   SIMPLE_ENGINE::SIMPLE_ENGINE(CORE::BODY_STATE_VEC body_states_ic,
                                CORE::DT dt,
                                int block_size,
                                std::optional<std::string> body_states_log_dir_opt) : ENGINE(std::move(body_states_ic), dt, std::move(body_states_log_dir_opt)),
                                                                                      block_size_(block_size)
   {
   }

   CORE::BODY_STATE_VEC generate_body_state_vec(const data_t_3d * h_X, const data_t_3d * h_V, const data_t * mass, const size_t nbody)
   {
      CORE::BODY_STATE_VEC body_states;
      body_states.reserve(nbody);
      for (size_t i_body = 0; i_body < nbody; i_body++)
      {
            CORE::POS pos_temp{h_X[i_body].x, h_X[i_body].y, h_X[i_body].z};
            CORE::VEL vel_temp{h_V[i_body].x, h_V[i_body].y, h_V[i_body].z};
            body_states.emplace_back(pos_temp, vel_temp, mass[i_body]);
      }
      return body_states;
   }

   CORE::BODY_STATE_VEC SIMPLE_ENGINE::execute(int n_iter)
   {
      CORE::TIMER timer("cuda program");

      /* BIN file of initial conditions */
      const auto &ic = body_states_ic();

      // TODO: get better debug message.
      size_t nBody = ic.size();

      // random initializer just for now
      srand(time(NULL));
      size_t vector_size = sizeof(data_t_3d) * nBody;
      size_t data_size = sizeof(data_t) * nBody;

      /*
     *   host side memory allocation
     */
      data_t_3d *h_X, *h_A, *h_V, *h_output_X;
      data_t *h_M;
      host_malloc_helper((void **)&h_X, vector_size);
      host_malloc_helper((void **)&h_A, vector_size);
      host_malloc_helper((void **)&h_V, vector_size);
      host_malloc_helper((void **)&h_output_X, vector_size);
      host_malloc_helper((void **)&h_M, data_size);
      timer.elapsed_previous("allocated host side memory");
      /*
     *   input randome initialize
     */

      parse_ic(h_X, h_V, h_M, ic);
      timer.elapsed_previous("deserialize_body_state_vec_from_csv");

      /*
     *  mass 
     */
      data_t *d_M;
      gpuErrchk(cudaMalloc((void **)&d_M, data_size));
      /*
     *   create double buffer on device side
     */
      data_t_3d **d_X, **d_A, **d_V;
      unsigned src_index = 0;
      unsigned dest_index = 1;
      d_X = (data_t_3d **)malloc(2 * sizeof(data_t_3d *));
      gpuErrchk(cudaMalloc((void **)&d_X[src_index], vector_size));
      gpuErrchk(cudaMalloc((void **)&d_X[dest_index], vector_size));

      d_A = (data_t_3d **)malloc(2 * sizeof(data_t_3d *));
      gpuErrchk(cudaMalloc((void **)&d_A[src_index], vector_size));
      gpuErrchk(cudaMalloc((void **)&d_A[dest_index], vector_size));

      d_V = (data_t_3d **)malloc(2 * sizeof(data_t_3d *));
      gpuErrchk(cudaMalloc((void **)&d_V[src_index], vector_size));
      gpuErrchk(cudaMalloc((void **)&d_V[dest_index], vector_size));

      timer.elapsed_previous("allocated device memory");
      /*
     *   create double buffer on device side
     */
      // cudaMemcpy(d_A[0], h_A, vector_size, cudaMemcpyHostToDevice);
      cudaMemcpy(d_X[src_index], h_X, vector_size, cudaMemcpyHostToDevice);
      cudaMemcpy(d_V[src_index], h_V, vector_size, cudaMemcpyHostToDevice);
      cudaMemcpy(d_M, h_M, data_size, cudaMemcpyHostToDevice);
      timer.elapsed_previous("copied input data from host to device");

      // nthread is assigned to either 32 by default or set to a custom power of 2 by user
      std::cout << "Set thread_per_block to " << block_size_ << std::endl;
      unsigned nblocks = (nBody + block_size_ - 1) / block_size_;

      // calculate the initialia acceleration
      calculate_acceleration<<<nblocks, block_size_>>>(nBody, d_X[src_index], d_M, d_A[src_index]);
      timer.elapsed_previous("Calculated initial acceleration");

      {
         CORE::TIMER core_timer("computation_core");
         for (int i_iter = 0; i_iter < n_iter; i_iter++)
         {
            // There should be more than one ways to do synchronization. I temporarily randomly choosed one
            calculate_acceleration<<<nblocks, block_size_>>>(nBody, d_X[src_index], d_M,                                                     //input
                                                            d_A[dest_index]);                                                               // output
            update_step<<<nblocks, block_size_>>>(nBody, (data_t)dt(), d_X[src_index], d_V[src_index], d_A[src_index], d_M, d_A[dest_index], //input
                                                   d_X[dest_index], d_V[dest_index]);                                                         // output

            // we don't have to synchronize here but this gices a better visualization on how fast / slow the program is
            cudaDeviceSynchronize();

            if(i_iter == 0) {
               push_body_states_to_log([&]() 
                  { return generate_body_state_vec(d_X[src_index], d_V[src_index], d_M, nBody); }); 
            }
            push_body_states_to_log([&]() 
               { return generate_body_state_vec(d_X[dest_index], d_V[dest_index], d_M, nBody); });
            
            if(i_iter % 10 == 0) {
               serialize_body_states_log();
            }
            swap(src_index, dest_index);

            timer.elapsed_previous(std::string("iter") + std::to_string(i_iter));

         }
         cudaDeviceSynchronize();
      }

      // at end, the final data is actually at src_index because the last swap
      cudaMemcpy(h_output_X, d_X[src_index], vector_size, cudaMemcpyDeviceToHost);
      timer.elapsed_previous("copied output back to host");
      // Just for debug purpose on small inputs
      // for (unsigned i = 0; i < nBody; i++)
      // {
      //    //printf("object = %d, %f, %f, %f\n", i, h_output_X[i].x, h_output_X[i].y, h_output_X[i].z);
      // }

      return body_states_ic();
   }
}