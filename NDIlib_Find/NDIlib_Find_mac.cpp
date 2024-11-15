#include <cstdio>
#include <chrono>
#include <Processing.NDI.Lib.h>

#ifdef _WIN32
#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x86.lib")
#endif // _WIN64
#endif // _WIN32

// RAII class for initializing and cleaning up NDI
class NDIInitializer {
public:
    NDIInitializer() {
        if (!NDIlib_initialize()) {
            throw std::runtime_error("Failed to initialize NDI");
        }
    }

    ~NDIInitializer() {
        NDIlib_destroy();
    }
};

// RAII class for handling the NDI find instance
class NDIFinder {
public:
    NDIFinder() {
        pNDI_find = NDIlib_find_create_v2();
        if (!pNDI_find) {
            throw std::runtime_error("Failed to create NDI finder");
        }
    }

    ~NDIFinder() {
        NDIlib_find_destroy(pNDI_find);
    }

    NDIlib_find_instance_t get() const {
        return pNDI_find;
    }

private:
    NDIlib_find_instance_t pNDI_find;
};

int main(int argc, char* argv[])
{
    try {
        // Initialize NDI using RAII
        NDIInitializer ndiInit;

        // Create NDI finder using RAII
        NDIFinder ndiFinder;

        // Run for one minute
        using namespace std::chrono;
        for (const auto start = high_resolution_clock::now(); high_resolution_clock::now() - start < minutes(1);) {
            // Wait up to 5 seconds to check for new sources to be added or removed
            if (!NDIlib_find_wait_for_sources(ndiFinder.get(), 5000 /* milliseconds */)) {
                printf("No change to the sources found.\n");
                continue;
            }

            // Get the updated list of sources
            uint32_t no_sources = 0;
            const NDIlib_source_t* p_sources = NDIlib_find_get_current_sources(ndiFinder.get(), &no_sources);

            // Display all the sources.
            printf("Network sources (%u found).\n", no_sources);
            for (uint32_t i = 0; i < no_sources; i++)
                printf("%u. %s\n", i + 1, p_sources[i].p_ndi_name);
        }
        
        // No explicit cleanup needed, RAII will take care of destroying resources
        return 0;
    }
    catch (const std::exception& e) {
        // Handle errors, print the message, and return a non-zero value
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
