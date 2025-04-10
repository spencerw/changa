#ifndef GPU_MEMORY_POOL_H
#define GPU_MEMORY_POOL_H

#include <cuda_runtime.h>
#include <stdio.h>
#include <map>
#include <vector>
#include <time.h>  // For timing analytics
#include "charm++.h"  // For CmiCreateLock, CmiLock, CmiUnlock, and CmiMyPe

/**
 * @brief A GPU memory pool class for C++03 with thread-local pools for each PE.
 * 
 * IMPORTANT NOTE: As of [current date], reused memory blocks are no longer zeroed out 
 * before being returned to the caller. This was changed to improve performance. Callers
 * must initialize the entire memory block before use to ensure deterministic behavior.
 */
class GPUMemoryPool {
public:
    // Get the pool for the current PE
    static GPUMemoryPool& getPoolForPE() {
        // Use the common pools vector from getPoolsVector
        static std::vector<GPUMemoryPool*>& pools = *(std::vector<GPUMemoryPool*>*)getPoolsVector();
        static CmiNodeLock poolsVectorLock = CmiCreateLock();  // Protect access to the pools vector
        int pe = CmiMyPe();
        
        // Lock the mutex before modifying the vector
        CmiLock(poolsVectorLock);
        
        // Resize vector if needed
        if (pools.size() <= pe) {
            pools.resize(pe + 1, NULL);
        }
        
        // Create pool for this PE if it doesn't exist
        if (pools[pe] == NULL) {
            pools[pe] = new GPUMemoryPool();
#ifdef GPU_POOL_VERBOSE
            printf("[GPUMemoryPool] Created new pool for PE %d\n", pe);
            fflush(stdout);
#endif
        }
        
        // Now that we're done, unlock
        CmiUnlock(poolsVectorLock);
        return *pools[pe];
    }
    
    // Drop-in wrapper for cudaMalloc
    cudaError_t malloc(void** ptr, size_t size, const char* file, int line);
    
    // Drop-in wrapper for cudaFree
    cudaError_t free(void* ptr, const char* file, int line);
    
    // Print analytics report
    void printAnalyticsReport();
    
    // Print global memory statistics across all PEs
    void printGlobalMemoryStats();
    
    // Reset analytics counters for a new timestep
    void resetAnalyticsCounters();
    
    // Static method to print analytics report
    static void printReport() {
        getPoolForPE().printAnalyticsReport();
    }
    
    // Static method to print global memory statistics
    static void printGlobalStats() {
        getPoolForPE().printGlobalMemoryStats();
    }
    
    // Static method to reset analytics counters for current PE
    static void resetCounters() {
        getPoolForPE().resetAnalyticsCounters();
    }
    
    // Static method to reset analytics counters for all PEs
    static void resetAllCounters() {
        static std::vector<GPUMemoryPool*>& pools = *(std::vector<GPUMemoryPool*>*)getPoolsVector();
        static CmiNodeLock poolsVectorLock = CmiCreateLock();  // Lock for pools vector access
        
#ifdef GPU_POOL_VERBOSE
        printf("[GPUMemoryPool] Resetting analytics counters for all PEs\n");
        fflush(stdout);
#endif
        
        // Protect access to the pools vector
        CmiLock(poolsVectorLock);
        
        // Reset counters in all pools
        for (size_t i = 0; i < pools.size(); i++) {
            if (pools[i] != NULL) {
                pools[i]->resetAnalyticsCounters();
            }
        }
        
        CmiUnlock(poolsVectorLock);
        
#ifdef GPU_POOL_VERBOSE
        printf("[GPUMemoryPool] All counters reset\n");
        fflush(stdout);
#endif
    }
    
    // Static method to print consolidated report for all pools
    static void printConsolidatedReport();
    
    // Static method to cleanup all pools at program termination
    static void cleanupAllPools() {
        static std::vector<GPUMemoryPool*>& pools = *(std::vector<GPUMemoryPool*>*)getPoolsVector();
        
        // Only PE0 should print messages, but all PEs should clean up
        if (CmiMyPe() == 0) {
#ifdef GPU_POOL_VERBOSE
            printf("[GPUMemoryPool] Cleaning up all memory pools at program termination\n");
            fflush(stdout);
#endif
            
            // Only PE0 prints reports
            // COMMENTED OUT: Removing detailed reporting during shutdown
            // printConsolidatedReport();
            
            // Also print detailed analytics if enabled
            // COMMENTED OUT: Removing detailed analytics during shutdown
            // printConsolidatedDetailedStats();
        }
        
        // Each PE cleans up its own pool
        int pe = CmiMyPe();
        if (pe < pools.size() && pools[pe] != NULL) {
            delete pools[pe];
            pools[pe] = NULL;
        }
        
        // Only PE0 should print the final message
        if (CmiMyPe() == 0) {
#ifdef GPU_POOL_VERBOSE
            printf("[GPUMemoryPool] All memory pools cleaned up\n");
            fflush(stdout);
#endif
        }
    }
    
    // Get the address of the static pools vector for cleanup
    static void* getPoolsVector() {
        static std::vector<GPUMemoryPool*> pools;
        return &pools;
    }
    
    // Enable memory validation (for debugging)
    void enableMemoryValidation(bool enable = true) {
        memoryValidationEnabled = enable;
#ifdef GPU_POOL_VERBOSE
        printf("[GPUMemoryPool][PE %d] Memory validation %s\n", 
               CmiMyPe(), enable ? "enabled" : "disabled");
        fflush(stdout);
#endif
    }

    // Check if memory validation is enabled
    bool isMemoryValidationEnabled() const {
        return memoryValidationEnabled;
    }

    // Static methods to control memory validation for current PE
    static void enableValidation(bool enable = true) {
        getPoolForPE().enableMemoryValidation(enable);
    }

    static bool isValidationEnabled() {
        return getPoolForPE().isMemoryValidationEnabled();
    }
    
    // Print detailed timing analytics (similar to Python script)
    void printDetailedAnalytics();
    
    // Static method to print detailed analytics for current PE
    static void printDetailedStats() {
        getPoolForPE().printDetailedAnalytics();
    }
    
    // Static method to print consolidated detailed analytics from all PEs
    static void printConsolidatedDetailedStats() {
        // Only PE0 should print reports
        if (CmiMyPe() != 0) {
            return;
        }
        
#if GPU_POOL_REPORTS == 0
        // Analytics reports disabled by compile-time flag
        return;
#endif

#ifdef GPU_POOL_VERBOSE
        printf("\n============ Consolidated Detailed GPU Memory Analytics ============\n");
        printf("Printing detailed analytics from PE0 only...\n");
        fflush(stdout);
#endif
        
        // Get PE0's pool and print its detailed analytics if enabled
        GPUMemoryPool& pe0Pool = getPoolForPE();
        
        if (pe0Pool.isDetailedAnalyticsEnabled()) {
            pe0Pool.printDetailedAnalytics();
        } else {
#ifdef GPU_POOL_VERBOSE
            printf("PE0 does not have detailed analytics enabled.\n");
            printf("Enable with gpuPoolEnableDetailedStats(true)\n");
#endif
        }
        
#ifdef GPU_POOL_VERBOSE
        printf("==================================================================\n");
        fflush(stdout);
#endif
    }
    
    // Enable/disable detailed analytics collection
    void enableDetailedAnalytics(bool enable = true) {
        detailedAnalyticsEnabled = enable;
#ifdef GPU_POOL_VERBOSE
        printf("[GPUMemoryPool][PE %d] Detailed analytics %s\n", 
               CmiMyPe(), enable ? "enabled" : "disabled");
        fflush(stdout);
#endif
    }
    
    // Check if detailed analytics is enabled
    bool isDetailedAnalyticsEnabled() const {
        return detailedAnalyticsEnabled;
    }
    
    // Static methods to control detailed analytics for current PE
    static void enableDetailedStats(bool enable = true) {
        getPoolForPE().enableDetailedAnalytics(enable);
    }
    
    // Static method to enable detailed analytics for all PEs
    static void enableDetailedStatsAllPEs(bool enable = true) {
        static std::vector<GPUMemoryPool*>& pools = *(std::vector<GPUMemoryPool*>*)getPoolsVector();
        static CmiNodeLock poolsVectorLock = CmiCreateLock();  // Lock for pools vector access
        
#ifdef GPU_POOL_VERBOSE
        printf("[GPUMemoryPool] %s detailed analytics for all PEs\n", 
               enable ? "Enabling" : "Disabling");
        fflush(stdout);
#endif
        
        // Protect access to the pools vector
        CmiLock(poolsVectorLock);
        
        // Enable/disable detailed analytics in all pools
        for (size_t i = 0; i < pools.size(); i++) {
            if (pools[i] != NULL) {
                pools[i]->enableDetailedAnalytics(enable);
            }
        }
        
        CmiUnlock(poolsVectorLock);
        
#ifdef GPU_POOL_VERBOSE
        printf("[GPUMemoryPool] Detailed analytics %s for all PEs\n", 
               enable ? "enabled" : "disabled");
        fflush(stdout);
#endif
    }
    
    static bool isDetailedStatsEnabled() {
        return getPoolForPE().isDetailedAnalyticsEnabled();
    }
    
    // Getter methods for analytics counters
    unsigned long getAllocations() const { return totalAllocations; }
    unsigned long getFrees() const { return totalFrees; }
    unsigned long getReuses() const { return totalReuses; }
    unsigned long getBytesAllocated() const { return totalBytesAllocated; }
    size_t getFreeBlocksCount() const { return freeBlocks.size(); }
    size_t getAllocatedSizesCount() const { return allocatedSizes.size(); }
    double getMallocTime() const { return totalMallocTime; }
    double getFreeTime() const { return totalFreeTime; }
    
private:
    GPUMemoryPool();
    ~GPUMemoryPool();
    GPUMemoryPool(const GPUMemoryPool&) {}
    GPUMemoryPool& operator=(const GPUMemoryPool&) { return *this; }
    
    // Map of free blocks keyed by their actual size.
    std::multimap<size_t, void*> freeBlocks;
    // Map to track allocated block sizes.
    std::map<void*, size_t> allocatedSizes;
    
    // Analytics data
    unsigned long totalAllocations;     // Total number of cudaMalloc calls
    unsigned long totalFrees;           // Total number of free operations
    unsigned long totalReuses;          // Total number of reused blocks
    unsigned long totalBytesAllocated;  // Total bytes allocated with cudaMalloc
    double totalMallocTime;             // Total time spent in malloc operations (seconds)
    double totalFreeTime;               // Total time spent in free operations (seconds)
    
    // Memory validation toggle - controls whether freed blocks are validated on reuse
    bool memoryValidationEnabled;
    
    // Structure for timing statistics
    struct OperationRecord {
        double timeInMicroseconds;
        size_t size;
    };
    
    // Detailed timing data for analytics
    std::vector<OperationRecord> newAllocationTimes;
    std::vector<OperationRecord> reuseTimes;
    std::vector<OperationRecord> freePoolTimes;
    bool detailedAnalyticsEnabled;
    
    // Helper methods for detailed analytics
    double calculateMean(const std::vector<double>& values);
    double calculateMedian(std::vector<double> values);
    double calculateStdDev(const std::vector<double>& values, double mean);
    double calculatePercentile(std::vector<double> values, double percentile);
    void reportOperationStats(const char* description, const std::vector<OperationRecord>& records);
    void reportStatsBySize(const char* description, const std::vector<OperationRecord>& records);
    
    // Helper method to get current time in seconds
    double getCurrentTimeSeconds();
};

// Templated inline wrapper to hide cast ugliness.
template <typename T>
inline cudaError_t gpuPoolMallocTyped(T** ptr, size_t size, const char* file, int line) {
    return GPUMemoryPool::getPoolForPE().malloc(reinterpret_cast<void**>(ptr), size, file, line);
}

// Convenience macros to automatically use __FILE__ and __LINE__
#define gpuPoolMalloc(ptr, size) gpuPoolMallocTyped(ptr, size, __FILE__, __LINE__)
#define gpuPoolFree(ptr) GPUMemoryPool::getPoolForPE().free(ptr, __FILE__, __LINE__)

// Convenience macro to print the memory pool report
#define gpuPoolPrintReport() GPUMemoryPool::printReport()

// Convenience macro to print global memory statistics
#define gpuPoolPrintGlobalStats() GPUMemoryPool::printGlobalStats()

// Convenience macro to reset analytics counters
#define gpuPoolResetCounters() GPUMemoryPool::resetCounters()

// Convenience macro to reset analytics counters for all PEs
#define gpuPoolResetAllCounters() GPUMemoryPool::resetAllCounters()

// New macro to print consolidated report across all PEs
#define gpuPoolPrintConsolidatedReport() GPUMemoryPool::printConsolidatedReport()

// Convenience macros for memory validation
#define gpuPoolEnableValidation(enable) GPUMemoryPool::enableValidation(enable)
#define gpuPoolIsValidationEnabled() GPUMemoryPool::isValidationEnabled()

// Convenience macro to print detailed memory pool statistics
#define gpuPoolPrintDetailedStats() GPUMemoryPool::printDetailedStats()

// Convenience macro to print consolidated detailed statistics from all PEs
#define gpuPoolPrintConsolidatedDetailedStats() GPUMemoryPool::printConsolidatedDetailedStats()

// Convenience macros for detailed analytics control
#define gpuPoolEnableDetailedStats(enable) GPUMemoryPool::enableDetailedStats(enable)
#define gpuPoolEnableDetailedStatsAllPEs(enable) GPUMemoryPool::enableDetailedStatsAllPEs(enable)
#define gpuPoolIsDetailedStatsEnabled() GPUMemoryPool::isDetailedStatsEnabled()

// Define for controlling debug output
// To enable verbose output, compile with -DGPU_POOL_VERBOSE added to your CXXFLAGS
// NOTE: We ensure the macro is always defined to avoid compilation errors
#ifdef GPU_POOL_VERBOSE
#define GPU_POOL_VERBOSE_PRINT(fmt, ...) CkPrintf("[GPUMemoryPool][PE %d] " fmt, CmiMyPe(), ##__VA_ARGS__)
#else
// Define as empty macro that does nothing (but is still defined)
#define GPU_POOL_VERBOSE_PRINT(fmt, ...) do {} while(0)
#endif

// Define for controlling analytics reports
// To disable analytics reports, compile with -DGPU_POOL_REPORTS=0
#ifndef GPU_POOL_REPORTS
#define GPU_POOL_REPORTS 1  // Enable reports by default
#endif

#endif // GPU_MEMORY_POOL_H 