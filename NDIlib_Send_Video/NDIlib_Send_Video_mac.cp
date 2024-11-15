#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>  // for std::fill
#include <memory>     // for std::unique_ptr
#include <iomanip>    // for std::setprecision
#include <Processing.NDI.Lib.h>

#ifdef _WIN32
#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x86.lib")
#endif // _WIN64
#endif // _WIN32

// RAII wrapper for NDI sender
class NDISender {
public:
    NDISender() {
        // Initialize the NDI library
        if (!NDIlib_initialize()) {
            throw std::runtime_error("Failed to initialize NDI library.");
        }

        // Create the NDI sender
        pNDI_send = NDIlib_send_create();
        if (!pNDI_send) {
            throw std::runtime_error("Failed to create NDI sender.");
        }
    }

    ~NDISender() {
        // Destroy the NDI sender and clean up
        if (pNDI_send) {
            NDIlib_send_destroy(pNDI_send);
        }
        NDIlib_destroy();  // Clean up NDI library
    }

    // Accessor for the NDI sender instance
    NDIlib_send_instance_t get() const {
        return pNDI_send;
    }

private:
    NDIlib_send_instance_t pNDI_send = nullptr;
};

// RAII wrapper for the NDI video frame
class NDIFrame {
public:
    NDIFrame(int xres, int yres)
        : xres(xres), yres(yres), frame_buffer(xres * yres * 4) {
        NDI_video_frame.xres = xres;
        NDI_video_frame.yres = yres;
        NDI_video_frame.FourCC = NDIlib_FourCC_type_BGRX;
        NDI_video_frame.p_data = frame_buffer.data();
    }

    // Accessor for the NDI video frame
    NDIlib_video_frame_v2_t* get() {
        return &NDI_video_frame;
    }

    // Buffer is managed by std::vector, so no need for manual memory management
    std::vector<uint8_t>& getBuffer() {
        return frame_buffer;
    }

private:
    int xres, yres;
    NDIlib_video_frame_v2_t NDI_video_frame;
    std::vector<uint8_t> frame_buffer; // RAII handles memory
};

int main(int argc, char* argv[]) {
    try {
        // Create NDI sender (RAII)
        NDISender ndiSender;

        // Create NDI video frame (RAII)
        NDIFrame videoFrame(1920, 1080);

        // Run for five minutes.
        using namespace std::chrono;
        for (const auto start = high_resolution_clock::now(); high_resolution_clock::now() - start < minutes(5);) {
            // Get the current time
            const auto start_send = high_resolution_clock::now();

            // Send 200 frames
            for (int idx = 200; idx; idx--) {
                // Fill the buffer. We alternate between black and white.
                std::fill(videoFrame.getBuffer().begin(), videoFrame.getBuffer().end(), (idx & 1) ? 255 : 0);

                // Submit the frame
                NDIlib_send_send_video_v2(ndiSender.get(), videoFrame.get());
            }

            // Display the FPS
            std::cout << "200 frames sent, at "
                      << std::fixed << std::setprecision(2) << (200.0f / duration_cast<duration<float>>(high_resolution_clock::now() - start_send).count())
                      << " fps" << std::endl;
        }

        // Everything is cleaned up automatically by RAII objects (NDISender and NDIFrame)
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
