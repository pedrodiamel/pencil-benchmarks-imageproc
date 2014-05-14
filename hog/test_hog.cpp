#include <chrono>
#include <random>

#include <opencv2/opencv.hpp>
//#include <opencv2/ocl/ocl.hpp>
//#include <opencv2/imgproc/imgproc.hpp>

#include "opencl.hpp"
#include "utility.hpp"
#include "hog.pencil.h"

void time_hog( const std::vector<carp::record_t>& pool, const std::vector<int>& sizes, int num_positions )
{
    carp::Timing timing("gaussian blur");

    std::mt19937 rng;   //uses default seed

    for ( auto & size : sizes ) {
        for ( auto & item : pool ) {
            cv::Mat cpu_gray;
            cv::cvtColor( item.cpuimg(), cpu_gray, CV_RGB2GRAY );

            std::vector<double> locations_x, locations_y;
            std::uniform_real_distribution<> gen(0, cpu_gray.cols-1);
            std::generate_n(std::back_inserter(locations_x), num_positions, [&](){ return gen(rng); });
            std::generate_n(std::back_inserter(locations_y), num_positions, [&](){ return gen(rng); });

            std::vector<double> cpu_result(HISTOGRAM_BINS), gpu_result(HISTOGRAM_BINS), pen_result(HISTOGRAM_BINS);
            std::chrono::duration<double> elapsed_time_cpu, elapsed_time_gpu_p_copy, elapsed_time_gpu_nocopy, elapsed_time_pencil;

            {
                //CPU implement
                const auto cpu_start = std::chrono::high_resolution_clock::now();

                //!!!TODO!!!

                const auto cpu_end = std::chrono::high_resolution_clock::now();
                elapsed_time_cpu = cpu_end - cpu_start;
                //Free up resources
            }
            {
                const auto gpu_compile_start = std::chrono::high_resolution_clock::now();
                const auto gpu_copy_start = std::chrono::high_resolution_clock::now();
                const auto gpu_start = std::chrono::high_resolution_clock::now();

                //!!!TODO!!!

                const auto gpu_end = std::chrono::high_resolution_clock::now();
                const auto gpu_copy_end = std::chrono::high_resolution_clock::now();
                const auto gpu_compile_end = std::chrono::high_resolution_clock::now();

                elapsed_time_gpu_p_copy = gpu_copy_end - gpu_copy_start;
                elapsed_time_gpu_nocopy = gpu_end      - gpu_start;
                auto elapsed_time_gpu_compile = gpu_compile_end - gpu_compile_start;
                //Free up resources
            }
            {
                const auto pencil_start = std::chrono::high_resolution_clock::now();
                pencil_hog( cpu_gray.rows, cpu_gray.cols, cpu_gray.step1(), cpu_gray.ptr<uint8_t>()
                          , num_positions, locations_x.data(), locations_y.data()
                          , size
                          , pen_result.data()
                          );

                const auto pencil_end = std::chrono::high_resolution_clock::now();
                elapsed_time_pencil = pencil_end - pencil_start;
                //Free up resources
            }
#if 0
            // Verifying the results
            if ( (cv::norm( cpu_result - gpu_result ) > 0.01) ||
                 (cv::norm( cpu_result - pen_result) > 0.01) )
            {
                PRINT(cv::norm(gpu_result - cpu_result));
                PRINT(cv::norm(pen_result - cpu_result));

                cv::Mat gpu_result8;
                cv::Mat cpu_result8;
                cv::Mat pen_result8;
                cv::Mat diff8;

                gpu_result.convertTo( gpu_result8, CV_8UC1, 255. );
                cpu_result.convertTo( cpu_result8, CV_8UC1, 255. );
                pen_result.convertTo( pen_result8, CV_8UC1, 255. );
                cv::Mat absdiff = cv::abs(pen_result - cpu_result);
                absdiff.convertTo( diff8, CV_8UC1, 255. );

                cv::imwrite( "gpu_gaussian.png", gpu_result8 );
                cv::imwrite( "cpu_gaussian.png", cpu_result8 );
                cv::imwrite( "pencil_gaussian.png", pen_result8 );
                cv::imwrite( "diff_gaussian.png", diff8 );

                throw std::runtime_error("The GPU results are not equivalent with the CPU results.");
            }
#endif
            timing.print( elapsed_time_cpu, elapsed_time_gpu_p_copy, elapsed_time_gpu_nocopy, elapsed_time_pencil );
        }
    }
}



int main(int argc, char* argv[])
{
    std::cout << "This executable is iterating over all the files which are present in the directory `./pool'. " << std::endl;

    auto pool = carp::get_pool("pool");
#ifdef RUN_ONLY_ONE_EXPERIMENT
    time_hog( pool, {64}, 50 );
#else
    time_hog( pool, {16, 32, 64, 128, 192}, 50 );
#endif

    return EXIT_SUCCESS;
} // main