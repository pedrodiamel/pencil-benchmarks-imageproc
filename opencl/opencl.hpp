// UjoImro, 2013

#ifndef __OPENCL_HPP__
#define __OPENCL_HPP__

#include <string>
#include <vector>
#include <CL/cl.h>
#include <fstream>
#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/smart_ptr.hpp>
#include <opencv2/core/core.hpp>

#include "cltypes.h"
#include "errors.hpp"


namespace carp {

    namespace opencl {
                
        namespace utility {            

            std::string
            readfile( const std::string & filename )
            {
                std::ifstream input( filename, std::ifstream::binary );
    
                if (!input) throw std::runtime_error("error: Couldn't open file `" + filename + "'" );
        
                input.seekg(0, input.end);
                int length = input.tellg();
                input.seekg(0, input.beg);

                boost::scoped_array<char> result_c_str(new char[length+1]);            
                input.read(result_c_str.get(), length);
                if (!input) throw std::runtime_error("error: Could not read file `" + filename + "'");
            
                std::string result(result_c_str.get());

                return result;
            } // readfile


            void
            checkerror(
                const cl_int & error,
                const std::string & filename = __FILE__,
                const int & line = __LINE__
                ) throw (std::exception&) {
                if ( error != CL_SUCCESS )
                    throw std::runtime_error( std::string("error: OpenCL: ") +
                                              carp::opencl::errors[error] +
                                              "; line: " + std::to_string(line) + " in file " + filename );
            } // checkerror

            // this function is equivalent to the OpenCL example in the NVidia repository
            int roundup( int group_size, int global_size ) {
                if (global_size < group_size) global_size = group_size;
                 
                int r = global_size % group_size;
                if(r == 0) 
                {
                    return global_size;
                } else 
                {
                    return global_size + group_size - r;
                }
            } // roundup
        

            template <class T0>
            std::vector<T0> roundup(
                const std::vector<T0> & group_sizes,
                const std::vector<T0> & global_sizes )
            {
                assert(group_sizes.size()==global_sizes.size());
                
                std::vector<T0> result(group_sizes.size());

                for (
                    auto group = group_sizes.begin(),
                        glob = global_sizes.begin(),
                        rup = result.begin();
                    group!= group_sizes.end();
                    group++, glob++, rup++
                    )
                    *rup = roundup( *group, *glob );
                
                return result;
                
            } // roundup for vectors
             
        } // namespace utility



        class buffer
        {
        private:
            size_t m_size;
            
        public:
            buffer(size_t size) : m_size(size) { }

            size_t size() { return m_size; }
            
        }; // class buffer 

        
        
        class kernel {
        private:

            template <class MT0>
            class preparator {
            private:
                void * m_ptr;
                size_t m_size;
                
            public:
                preparator( MT0 & mt0 ) : m_ptr(reinterpret_cast<void*>(&mt0)), m_size(sizeof(mt0)) { }                
                void * ptr() { return m_ptr; } // ptr
                size_t size() { return m_size; } // size                 
            }; // class preparator
            
            cl_kernel cqKernel;
            cl_command_queue cqCommandQueue;
            bool m_set;

            template <class MT0, class Pos>
            bool
            setparameter( Pos & pos, MT0 & mt0 ) {
                assert(cqKernel);

                kernel::preparator<MT0> parameter(mt0);                
                
                utility::checkerror( clSetKernelArg(cqKernel, pos, parameter.size(), parameter.ptr() ), __FILE__, __LINE__ );
                pos++; // move the position of the parameter applied
                return true;
            } // setparameter
                        
        public:
            kernel() : cqKernel(NULL), cqCommandQueue(NULL), m_set(false) { };

            void
            set( const cl_kernel & cqKernel, const cl_command_queue & cqCommandQueue ) {
                this->cqKernel = cqKernel;
                this->cqCommandQueue = cqCommandQueue;
                this->m_set = true;
                                
            } // operator set

            void release() {
                assert(m_set);
                if (cqKernel) clReleaseKernel(cqKernel);
                cqKernel=NULL;                
            } // release

            template <class ...MT>
            kernel & operator() ( MT ... mt ) {
                assert(m_set);
                int pos=0; // the position of the parameters
                std::vector<bool> err { setparameter(pos, mt)... };

                return *this;
            } // operator ()
            
            void
            groupsize( std::vector<size_t> groupsize, std::vector<size_t> worksize ) {
                assert(m_set);
                assert(cqKernel);
                assert(cqCommandQueue);
                std::vector<size_t> kernelsize = utility::roundup(groupsize, worksize);
                utility::checkerror(clEnqueueNDRangeKernel( cqCommandQueue, cqKernel, worksize.size(), NULL, &(kernelsize[0]), &(groupsize[0]), 0, NULL, NULL ), __FILE__, __LINE__ );
                utility::checkerror(clFinish(cqCommandQueue), __FILE__, __LINE__ );
            } // groupsize 
            
            
        }; // class kernel

        template <>
        class carp::opencl::kernel::preparator<opencl::buffer> {
        private:
            void * m_ptr;
            size_t m_size;
                
        public:
            preparator( opencl::buffer & mt0 ) : m_ptr(NULL), m_size(mt0.size()) { }
            void * ptr() { return m_ptr; } // ptr
            size_t size() { return m_size; } // size                 
        }; // class preparator <opencl::buffer>

        
        class device {
        private:
            typedef std::map<std::string, opencl::kernel> kernels_t;            
            
            cl_context cxGPUContext;        // OpenCL context
            cl_command_queue cqCommandQueue;// OpenCL command queue
            cl_platform_id cpPlatform;      // OpenCL platform
            cl_device_id cdDevice;          // OpenCL device
            cl_program cpProgram;           // OpenCL program
            kernels_t kernels; // OpenCL kernel            
            cl_uint num_platforms;          // The number of OpenCL platforms
            cl_uint num_devices;            // The number of OpenCL devices

            device( const device & ){ } // device is not copyable
            
        public:

            device() {
                cl_int err; // for the error messages

                utility::checkerror(clGetPlatformIDs(1, &cpPlatform, &num_platforms), __FILE__, __LINE__ );
                assert(num_platforms==1);  // there is only one supported platform at the time
                
                utility::checkerror(clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 1, &cdDevice, &num_devices), __FILE__, __LINE__ );
                assert(num_devices==1);

                cxGPUContext = clCreateContext( NULL, 1, &cdDevice, NULL, NULL, &err );
                utility::checkerror( err, __FILE__, __LINE__ );
                
                cqCommandQueue = clCreateCommandQueue( cxGPUContext, cdDevice, /*CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE*/ 0,  &err ); // kernels are executed in order
                utility::checkerror( err, __FILE__, __LINE__ );
                
            } // device
                
            ~device() {
                if(cpProgram) clReleaseProgram(cpProgram);
                if(cqCommandQueue) clReleaseCommandQueue(cqCommandQueue);
                if(cxGPUContext) clReleaseContext(cxGPUContext);
                std::for_each( kernels.begin(), kernels.end(), [] ( kernels_t::allocator_type::value_type & kernel ) {// kernel->second.release();
                    } );
            } // ~device

            void compile( const std::vector<std::string> & filenames,
                          const std::vector<std::string> & kernelnames ) {
                cl_int err;
                
                // loading the sources
                std::vector<std::string> sources;
                std::vector<const char*> c_strs;
                std::vector<size_t> lengths;                
                
                for( const std::string & filename : filenames ) {                    
                    sources.push_back(utility::readfile(filename));
                    c_strs.push_back(sources.back().c_str());
                    lengths.push_back(sources.back().size());                    
                }
                                
                cpProgram = clCreateProgramWithSource( cxGPUContext, sources.size(), &(c_strs[0]), &(lengths[0]), &err );
                utility::checkerror(err, __FILE__, __LINE__ );

                // building the OpenCL source code
                err = clBuildProgram( cpProgram, 0, NULL, NULL, NULL, NULL );
                if ( err!=CL_SUCCESS )
                {
                    size_t len;
                    std::string buffer(1048576, 0);

                    utility::checkerror( clGetProgramBuildInfo( cpProgram, cdDevice, CL_PROGRAM_BUILD_LOG, buffer.size(), reinterpret_cast<void*>(&buffer[0]), &len ), __FILE__, __LINE__ );

                    std::cerr << buffer << std::endl;
                    throw std::runtime_error("error: OpenCL: The compilation has failed.");                    
                }

                // extracting the kernel entrances
                for( const std::string & kernel_name : kernelnames )
                {
                    cl_int err;
                    kernels[kernel_name].set( clCreateKernel( cpProgram, kernel_name.c_str(), &err ), cqCommandQueue );
                    utility::checkerror( err, __FILE__, __LINE__ );
                }

                return;
            } // compile

            kernel & operator [] ( const std::string & filename ) {
                return kernels[filename];
            } // operator[]

            cl_context
            get_context() {
                return cxGPUContext;
            }

            cl_command_queue
            get_queue() {
                return cqCommandQueue;
            }
            
            
        }; // class device


        template <class T0>
        class array {
        private:
            cl_mem cl_ptr;
            size_t m_size;
            cl_context cqContext;
            cl_command_queue cqCommandQueue;
            
            array( const array<T0> & ) { } // copy constructor forbidden
            
        public:
            
            array( const cl_context & cqContext,
                   const cl_command_queue & cqCommandQueue,
                   const size_t & size,
                   cl_mem_flags flags = CL_MEM_READ_WRITE
                ) : m_size(size), cqContext(cqContext), cqCommandQueue(cqCommandQueue) {
                assert( (flags & CL_MEM_USE_HOST_PTR) == 0 );
                cl_int err;                
                cl_ptr = clCreateBuffer( cqContext, flags, size * sizeof(T0), NULL, &err );
                utility::checkerror( err, __FILE__, __LINE__ );              
            } // array

            array( const cl_context & cqContext,
                   const cl_command_queue & cqCommandQueue,
                   std::vector<T0> & input,
                   cl_mem_flags flags = CL_MEM_READ_WRITE
                ) : m_size(input.size()) {
                cl_int err;
                cl_ptr = clCreateBuffer( cqContext, flags | CL_MEM_COPY_HOST_PTR, size * sizeof(T0), reinterpret_cast<void*>(input[0]), &err );
                utility::checkerror( err, __FILE__, __LINE__ );
            } // array

            cl_mem cl() {
                return cl_ptr;
            } // cl
                        
            ~array() {
                clReleaseMemObject(cl_ptr);
            } // ~array

            size_t size() {
                return m_size;
            } // size
            
            std::vector<T0> get( ) {
                std::vector<T0> result(size);                
                utility::checkerror( clEnqueueReadBuffer( cqCommandQueue, cl_ptr, CL_TRUE, 0, sizeof(T0) * size, &(result[0]), 0, NULL, NULL), __FILE__, __LINE__ );

                return result;                
            } // extract
            
        }; // class array
        

        template <class T0>
        class image {
        public:
            typedef T0 value_type;
            
        private:
            int m_rows;
            int m_cols;
            cl_context cqContext;
            cl_command_queue cqCommandQueue;
            opencl::array<value_type> buf;

            image( const image<T0> & ) { }
            
        public:
            image( const cl_context & cqContext,
                   const cl_command_queue & cqCommandQueue,
                   int m_rows,
                   int m_cols ) :
                cqContext(cqContext),
                cqCommandQueue(cqCommandQueue),
                m_rows(rows),
                m_cols(cols),
                buf(cqContext, cqCommandQueue, m_rows * m_cols )
                { }

            image( opencl::device & device,
                   int rows,
                   int cols ) :
                cqContext(device.get_context()),
                cqCommandQueue(device.get_queue()),
                m_rows(rows),
                m_cols(cols),
                buf(cqContext, cqCommandQueue, rows * cols )
                { }

            int rows() const { return m_rows; };

            int cols() const { return m_cols; };

            clMat
            cl() {
                assert(buf.cl());                
                clMat result = { rows(), cols(), cols(), 0 };

                return result;
            }; // clMat

            cl_mem
            ptr() {
                assert(buf.cl());                
                return buf.cl();                
            } // ptr
            

            cv::Mat_<value_type> get() {
                cv::Mat_<value_type> result(rows(), cols());
                assert(result.isContinuous());

                utility::checkerror(
                    clEnqueueReadBuffer( cqCommandQueue, ptr(), CL_TRUE, 0, buf.size() * sizeof(value_type), reinterpret_cast<void*>(&result(0,0)), 0, NULL, NULL ) );
                
                return result;
            } // get

            void set( cv::Mat_<value_type> & image ) {
                assert(image.isContinuous());

                utility::checkerror(
                    clEnqueueWriteBuffer(
                        cqCommandQueue, ptr(), CL_TRUE, 0, buf.size() * sizeof(value_type), reinterpret_cast<void*>(&image(0,0)), 0, NULL, NULL ) );
                                
                return;                
            } // set
            
                
        }; // class image
        
            
    } // namespace opencl
    
} // namespace carp



#endif /* __OPENCL_HPP__ */
// LuM end of file
