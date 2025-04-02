#include "GPUMemoryPool.h"
#include <time.h>  // For time_t and related functions
#include <algorithm> // For std::sort and std::min
#include <cmath> // For sqrt

GPUMemoryPool::GPUMemoryPool() {
    double startTime = getCurrentTimeSeconds();
    
    // Add early flush to ensure output appears
    GPU_POOL_VERBOSE_PRINT("Initializing memory pool...\n");
    fflush(stdout);
    
    // Initialize analytics counters
    totalAllocations = 0;
    totalFrees = 0;
    totalReuses = 0;
    totalBytesAllocated = 0;
    totalMallocTime = 0.0;
    totalFreeTime = 0.0;
    
    // Initialize validation flag to false by default for better performance
    memoryValidationEnabled = false;
    
    // Initialize detailed analytics flag to false by default to avoid excessive memory usage
    detailedAnalyticsEnabled = false;
    
    // Clear detailed analytics vectors
    newAllocationTimes.clear();
    reuseTimes.clear();
    freePoolTimes.clear();
    
    double elapsedTimeMicros = (getCurrentTimeSeconds() - startTime) * 1000000.0;
    GPU_POOL_VERBOSE_PRINT("Initialized memory pool. [%.2f μs]\n", elapsedTimeMicros);
    fflush(stdout);
}

GPUMemoryPool::~GPUMemoryPool() {
    double startTime = getCurrentTimeSeconds();
    
    // COMMENTED OUT: Print final analytics report
    // printAnalyticsReport();
    
    // Free all stored memory blocks
    size_t blockCount = 0;
    for (std::multimap<size_t, void*>::iterator it = freeBlocks.begin(); 
         it != freeBlocks.end(); ++it) {
        // Add simple error handling for CUDA operations
        cudaError_t result = cudaFree(it->second);
        if (result != cudaSuccess) {
            // Just log the error but continue cleanup
            CkPrintf("[GPUMemoryPool][PE %d] ERROR freeing block at %p: %s\n", 
                    CmiMyPe(), it->second, cudaGetErrorString(result));
            fflush(stdout);
        }
        blockCount++;
    }
    freeBlocks.clear();
    allocatedSizes.clear();
    
    double elapsedTimeMicros = (getCurrentTimeSeconds() - startTime) * 1000000.0;
    GPU_POOL_VERBOSE_PRINT("Destroyed memory pool, freed %zu blocks [%.2f μs]\n", 
            blockCount, elapsedTimeMicros);
    fflush(stdout);
}

double GPUMemoryPool::getCurrentTimeSeconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

cudaError_t GPUMemoryPool::malloc(void** ptr, size_t size, const char* file, int line) {
    double startTime = getCurrentTimeSeconds();
    
    // Attempt to find a free block that's at least the required size
    std::multimap<size_t, void*>::iterator it = freeBlocks.lower_bound(size);
    if (it != freeBlocks.end()) {
        *ptr = it->second;
        size_t blockSize = it->first;
        
        // Memory validation - check if block contains unexpected values
        if (blockSize > 0 && memoryValidationEnabled) {
            // This is a costly operation but useful for debugging
            unsigned char* testBuf = new unsigned char[100];
            if (testBuf) {
                cudaMemcpy(testBuf, *ptr, std::min(blockSize, (size_t)100), cudaMemcpyDeviceToHost);
                cudaError_t err = cudaGetLastError();
                if (err != cudaSuccess) {
                    double elapsedTimeMicros = (getCurrentTimeSeconds() - startTime) * 1000000.0;
                    CkPrintf("[GPUMemoryPool][PE %d] WARNING: Memory validation error on reused block %p: %s [%.2f μs]\n", 
                           CmiMyPe(), *ptr, cudaGetErrorString(err), elapsedTimeMicros);
                    fflush(stdout);
                }
                delete[] testBuf;
            }
        }
        
        freeBlocks.erase(it);
        
        // Record the block size in allocatedSizes so it's tracked for future frees
        allocatedSizes[*ptr] = blockSize;
        
        // Update analytics
        totalReuses++;
        
        // PERFORMANCE OPTIMIZATION: We're removing the cudaMemset operation to improve reuse performance
        // The caller is expected to fully initialize the memory before use
        // Previous code: cudaMemset(*ptr, 0, size);
        
        double elapsedTimeMicros = (getCurrentTimeSeconds() - startTime) * 1000000.0;
        
        // Collect detailed analytics if enabled
        if (detailedAnalyticsEnabled) {
            OperationRecord record;
            record.timeInMicroseconds = elapsedTimeMicros;
            record.size = blockSize;
            reuseTimes.push_back(record);
        }
        
        // Add diagnostic message for reuse with timing information
        GPU_POOL_VERBOSE_PRINT("Reusing free block at %p of size %zu (requested %zu) [%s:%d] [%.2f μs]\n",
               *ptr, blockSize, size, file, line, elapsedTimeMicros);
        fflush(stdout);
        
        totalMallocTime += (getCurrentTimeSeconds() - startTime);
        return cudaSuccess;
    }
    
    // No suitable block found: fall back to cudaMalloc.
    cudaError_t err = cudaMalloc(ptr, size);
    double elapsedTimeMicros = (getCurrentTimeSeconds() - startTime) * 1000000.0;
    
    if (err == cudaSuccess) {
        GPU_POOL_VERBOSE_PRINT("Allocated new block at %p of size %zu using cudaMalloc [%s:%d] [%.2f μs]\n",
               *ptr, size, file, line, elapsedTimeMicros);
        fflush(stdout);
        // Record the allocated size.
        allocatedSizes[*ptr] = size;
        
        // Update analytics
        totalAllocations++;
        totalBytesAllocated += size;
        
        // Collect detailed analytics if enabled
        if (detailedAnalyticsEnabled) {
            OperationRecord record;
            record.timeInMicroseconds = elapsedTimeMicros;
            record.size = size;
            newAllocationTimes.push_back(record);
        }
    } else {
        CkPrintf("[GPUMemoryPool][PE %d] CUDA ERROR: cudaMalloc failed for size %zu at %s:%d - Error: %s [%.2f μs]\n", 
               CmiMyPe(), size, file, line, cudaGetErrorString(err), elapsedTimeMicros);
        fflush(stdout);
    }
    totalMallocTime += (getCurrentTimeSeconds() - startTime);
    return err;
}

cudaError_t GPUMemoryPool::free(void* ptr, const char* file, int line) {
    if (ptr == NULL) return cudaSuccess;
    
    double startTime = getCurrentTimeSeconds();
    
    // Look up the size of the allocated block.
    std::map<void*, size_t>::iterator it = allocatedSizes.find(ptr);
    if (it != allocatedSizes.end()) {
        size_t size = it->second;
        
        // Special handling for very large blocks - don't cache them to avoid OOM
        if (size > 1024*1024) { // 1MB threshold
            allocatedSizes.erase(it);
            cudaError_t result = cudaFree(ptr);
            double elapsedTimeMicros = (getCurrentTimeSeconds() - startTime) * 1000000.0;
            GPU_POOL_VERBOSE_PRINT("Direct freeing large block at %p of size %zu [%s:%d] [%.2f μs]\n",
                   ptr, size, file, line, elapsedTimeMicros);
            fflush(stdout);
            totalFreeTime += (getCurrentTimeSeconds() - startTime);
            return result;
        }
        
        // Add block to free list for future reuse
        freeBlocks.insert(std::make_pair(size, ptr));
        allocatedSizes.erase(it);
        
        // Update analytics
        totalFrees++;
        
        double elapsedTimeMicros = (getCurrentTimeSeconds() - startTime) * 1000000.0;
        
        // Collect detailed analytics if enabled
        if (detailedAnalyticsEnabled) {
            OperationRecord record;
            record.timeInMicroseconds = elapsedTimeMicros;
            record.size = size;
            freePoolTimes.push_back(record);
        }
        
        // We could add a debug print here if you want to track successful frees
        GPU_POOL_VERBOSE_PRINT("Added block to free pool at %p of size %zu [%s:%d] [%.2f μs]\n",
               ptr, size, file, line, elapsedTimeMicros);
        fflush(stdout);
        
        totalFreeTime += (getCurrentTimeSeconds() - startTime);
        return cudaSuccess;
    } else {
        // We're trying to free a pointer we didn't allocate.
        // Let cudaFree handle it, but record it for debugging.
        cudaError_t result = cudaFree(ptr);
        double elapsedTimeMicros = (getCurrentTimeSeconds() - startTime) * 1000000.0;
        CkPrintf("[GPUMemoryPool][PE %d] WARNING: Trying to free untracked block %p [%s:%d] [%.2f μs]\n", 
               CmiMyPe(), ptr, file, line, elapsedTimeMicros);
        fflush(stdout);
        
        totalFreeTime += (getCurrentTimeSeconds() - startTime);
        return result;
    }
}

void GPUMemoryPool::printAnalyticsReport() {
#if GPU_POOL_REPORTS == 0
    // Analytics reports disabled by compile-time flag
    return;
#endif

    // Get current time for timestamping the report
    time_t now;
    time(&now);
    char timeBuffer[80];
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    CkPrintf("\n==================================================\n");
    CkPrintf("GPUMemoryPool Analytics Report [PE %d] - Generated at %s\n", CmiMyPe(), timeBuffer);
    CkPrintf("==================================================\n");
    CkPrintf("Memory validation: %s\n", memoryValidationEnabled ? "ENABLED" : "DISABLED");
    CkPrintf("Total cudaMalloc allocations: %lu\n", totalAllocations);
    CkPrintf("Total memory blocks freed: %lu\n", totalFrees);
    CkPrintf("Total memory blocks reused: %lu\n", totalReuses);
    
    // Calculate average allocation size
    double avgAllocationSize = 0.0;
    if (totalAllocations > 0) {
        avgAllocationSize = (double)totalBytesAllocated / totalAllocations;
    }
    CkPrintf("Total bytes allocated: %lu bytes (%.2f MB)\n", 
           totalBytesAllocated, totalBytesAllocated / (1024.0 * 1024.0));
    CkPrintf("Average allocation size: %.2f bytes (%.2f KB)\n", 
           avgAllocationSize, avgAllocationSize / 1024.0);
    
    // Timing information
    CkPrintf("Total time spent in malloc operations: %.6f seconds\n", totalMallocTime);
    CkPrintf("Total time spent in free operations: %.6f seconds\n", totalFreeTime);
    CkPrintf("Total memory management time: %.6f seconds\n", totalMallocTime + totalFreeTime);
    
    // Current pool status
    CkPrintf("Current free blocks in pool: %lu\n", freeBlocks.size());
    
    // Calculate total memory currently in the pool
    size_t totalPoolMemory = 0;
    for (std::multimap<size_t, void*>::iterator it = freeBlocks.begin(); 
         it != freeBlocks.end(); ++it) {
        totalPoolMemory += it->first;
    }
    CkPrintf("Total memory in free pool: %lu bytes (%.2f MB)\n", 
           totalPoolMemory, totalPoolMemory / (1024.0 * 1024.0));
    
    CkPrintf("\nNote: This report shows data since the last counter reset (beginning of timestep).\n");
    CkPrintf("==================================================\n\n");
    fflush(stdout);
}

void GPUMemoryPool::printConsolidatedReport() {
    // In a multi-PE environment, we need a way to collect pools from all PEs
    // Since we can't directly access the static vector from getPoolForPE,
    // we'll use a different approach
    
    // Only PE0 should print the report to avoid issues in multi-node runs
    if (CmiMyPe() != 0) {
        return; // Non-PE0 processors exit immediately
    }
    
#if GPU_POOL_REPORTS == 0
    // Analytics reports disabled by compile-time flag
    return;
#endif

    // Get current time for timestamping the report
    time_t now;
    time(&now);
    char timeBuffer[80];
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // First, get the current PE's pool 
    int myPE = CmiMyPe(); // Should be 0 here
    GPUMemoryPool& myPool = getPoolForPE();
    
    // Get access to all pools - add protection
    static std::vector<GPUMemoryPool*>& pools = *(std::vector<GPUMemoryPool*>*)getPoolsVector();
    static CmiNodeLock poolsVectorLock = CmiCreateLock();  // Lock for pools vector access
    
    // Protect all pool vector access in this function with the lock
    CmiLock(poolsVectorLock);
    
    // Print the report header
    CkPrintf("\n===== GPU Memory Pool Report - Generated at %s =====\n", timeBuffer);
    fflush(stdout);
    
    // Print PE0's stats
    CkPrintf("PE 0 Statistics:\n");
    CkPrintf("  Allocations: %lu\n", myPool.totalAllocations);
    CkPrintf("  Frees: %lu\n", myPool.totalFrees);
    CkPrintf("  Reuses: %lu\n", myPool.totalReuses);
    CkPrintf("  Total operations: %lu\n", myPool.totalAllocations + myPool.totalFrees + myPool.totalReuses);
    CkPrintf("  Total bytes allocated: %lu (%.2f MB)\n", 
           myPool.totalBytesAllocated, myPool.totalBytesAllocated / (1024.0 * 1024.0));
    CkPrintf("  Free blocks: %zu\n", myPool.freeBlocks.size());
    CkPrintf("  Tracked allocations: %zu\n", myPool.allocatedSizes.size());
    CkPrintf("  Total malloc time: %.6f sec\n", myPool.totalMallocTime);
    CkPrintf("  Total free time: %.6f sec\n", myPool.totalFreeTime);
    fflush(stdout);
    
    CkPrintf("\nNote: This report shows data from PE0 only in multi-node runs.\n");
    CkPrintf("=========================================\n");
    
    // Release the lock
    CmiUnlock(poolsVectorLock);
    
    fflush(stdout);
}

void GPUMemoryPool::resetAnalyticsCounters() {
    double startTime = getCurrentTimeSeconds();
    
    GPU_POOL_VERBOSE_PRINT("Resetting analytics counters for new timestep\n");
    fflush(stdout);
    
    // Reset all analytics counters
    totalAllocations = 0;
    totalFrees = 0;
    totalReuses = 0;
    totalBytesAllocated = 0;
    totalMallocTime = 0.0;
    totalFreeTime = 0.0;
    
    // Clear detailed analytics vectors if enabled
    if (detailedAnalyticsEnabled) {
        newAllocationTimes.clear();
        reuseTimes.clear();
        freePoolTimes.clear();
    }
    
    double elapsedTimeMicros = (getCurrentTimeSeconds() - startTime) * 1000000.0;
    GPU_POOL_VERBOSE_PRINT("Analytics counters reset complete [%.2f μs]\n", elapsedTimeMicros);
    fflush(stdout);
}

// Add this new method to print global memory statistics

void GPUMemoryPool::printGlobalMemoryStats() {
#if GPU_POOL_REPORTS == 0
    // Analytics reports disabled by compile-time flag
    return;
#endif

    // Get access to all pools
    static std::vector<GPUMemoryPool*>& pools = *(std::vector<GPUMemoryPool*>*)getPoolsVector();
    static CmiNodeLock poolsVectorLock = CmiCreateLock();  // Lock for pools vector access
    
    // Protect all pool vector access with the lock
    CmiLock(poolsVectorLock);
    
    // Calculate global statistics
    unsigned long totalBytesAllocatedGlobal = 0;
    unsigned long totalMemoryOperations = 0;
    unsigned long totalFreeBlocksGlobal = 0;
    unsigned long totalAllocationsGlobal = 0;
    unsigned long totalFreesGlobal = 0;
    unsigned long totalReusesGlobal = 0;
    double totalMallocTimeGlobal = 0.0;
    double totalFreeTimeGlobal = 0.0;
    size_t activePECount = 0;
    
    for (size_t i = 0; i < pools.size(); i++) {
        if (pools[i] != NULL) {
            activePECount++;
            totalBytesAllocatedGlobal += pools[i]->totalBytesAllocated;
            totalMemoryOperations += pools[i]->totalAllocations + 
                                    pools[i]->totalFrees + 
                                    pools[i]->totalReuses;
            totalFreeBlocksGlobal += pools[i]->freeBlocks.size();
            totalAllocationsGlobal += pools[i]->totalAllocations;
            totalFreesGlobal += pools[i]->totalFrees;
            totalReusesGlobal += pools[i]->totalReuses;
            totalMallocTimeGlobal += pools[i]->totalMallocTime;
            totalFreeTimeGlobal += pools[i]->totalFreeTime;
        }
    }
    
    // We can unlock now as we're done accessing the pools vector
    CmiUnlock(poolsVectorLock);
    
    CkPrintf("\n============ Global GPU Memory Statistics ============\n");
    CkPrintf("Active PEs: %zu\n", activePECount);
    CkPrintf("Total bytes allocated across all PEs: %lu (%.2f MB)\n", 
           totalBytesAllocatedGlobal, totalBytesAllocatedGlobal / (1024.0 * 1024.0));
    CkPrintf("Total memory operations: %lu\n", totalMemoryOperations);
    CkPrintf("  - New allocations: %lu\n", totalAllocationsGlobal);
    CkPrintf("  - Memory frees: %lu\n", totalFreesGlobal);
    CkPrintf("  - Block reuses: %lu\n", totalReusesGlobal);
    CkPrintf("Total free blocks in pools: %lu\n", totalFreeBlocksGlobal);
    
    if (activePECount > 0) {
        CkPrintf("Average bytes allocated per PE: %.2f MB\n", 
               (totalBytesAllocatedGlobal / activePECount) / (1024.0 * 1024.0));
        CkPrintf("Average memory operations per PE: %.2f\n", 
               (double)totalMemoryOperations / activePECount);
    }
    
    CkPrintf("Total malloc time across all PEs: %.6f seconds\n", totalMallocTimeGlobal);
    CkPrintf("Total free time across all PEs: %.6f seconds\n", totalFreeTimeGlobal);
    CkPrintf("Total memory management time: %.6f seconds\n", totalMallocTimeGlobal + totalFreeTimeGlobal);
    
    // Calculate memory allocation distribution
    if (activePECount > 1) {
        // Calculate standard deviation of memory allocation
        double meanBytesAllocated = (double)totalBytesAllocatedGlobal / activePECount;
        double sumSquaredDiff = 0.0;
        
        for (size_t i = 0; i < pools.size(); i++) {
            if (pools[i] != NULL) {
                double diff = (double)pools[i]->totalBytesAllocated - meanBytesAllocated;
                sumSquaredDiff += diff * diff;
            }
        }
        
        double stdDeviation = sqrt(sumSquaredDiff / activePECount);
        double coefficientOfVariation = (meanBytesAllocated > 0) ? 
                                        (stdDeviation / meanBytesAllocated) * 100.0 : 0.0;
        
        CkPrintf("Memory allocation distribution:\n");
        CkPrintf("  - Standard deviation: %.2f MB\n", stdDeviation / (1024.0 * 1024.0));
        CkPrintf("  - Coefficient of variation: %.2f%%\n", coefficientOfVariation);
        CkPrintf("  - Imbalance indicator: %s\n", 
               (coefficientOfVariation > 50.0) ? "HIGH" : 
               (coefficientOfVariation > 20.0) ? "MEDIUM" : "LOW");
    }
    
    CkPrintf("==================================================\n");
    fflush(stdout);
}

// Helper functions for detailed analytics

double GPUMemoryPool::calculateMean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < values.size(); i++) {
        sum += values[i];
    }
    return sum / values.size();
}

double GPUMemoryPool::calculateMedian(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    size_t n = values.size();
    if (n % 2 == 0) {
        return (values[n/2 - 1] + values[n/2]) / 2.0;
    } else {
        return values[n/2];
    }
}

double GPUMemoryPool::calculateStdDev(const std::vector<double>& values, double mean) {
    if (values.size() < 2) return 0.0;
    double variance = 0.0;
    for (size_t i = 0; i < values.size(); i++) {
        double diff = values[i] - mean;
        variance += diff * diff;
    }
    return std::sqrt(variance / values.size());
}

double GPUMemoryPool::calculatePercentile(std::vector<double> values, double percentile) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    double index = percentile * (values.size() - 1) / 100.0;
    size_t lowerIndex = static_cast<size_t>(index);
    double fraction = index - lowerIndex;
    if (lowerIndex + 1 < values.size()) {
        return values[lowerIndex] * (1.0 - fraction) + values[lowerIndex + 1] * fraction;
    } else {
        return values[lowerIndex];
    }
}

void GPUMemoryPool::reportOperationStats(const char* description, const std::vector<OperationRecord>& records) {
    if (records.empty()) {
        CkPrintf("\n--- %s Statistics ---\n", description);
        CkPrintf("No data found\n");
        return;
    }
    
    // Extract just the times from the records
    std::vector<double> times;
    for (size_t i = 0; i < records.size(); i++) {
        times.push_back(records[i].timeInMicroseconds);
    }
    
    double sum = 0.0;
    for (size_t i = 0; i < times.size(); i++) {
        sum += times[i];
    }
    
    double mean = calculateMean(times);
    double median = calculateMedian(times);
    double stdDev = calculateStdDev(times, mean);
    
    CkPrintf("\n--- %s Statistics ---\n", description);
    CkPrintf("Count: %zu\n", times.size());
    CkPrintf("Total time: %.2f μs\n", sum);
    CkPrintf("Mean time: %.2f μs\n", mean);
    CkPrintf("Median time: %.2f μs\n", median);
    
    if (times.size() >= 2) {
        CkPrintf("Standard deviation: %.2f μs\n", stdDev);
    } else {
        CkPrintf("Not enough data for standard deviation\n");
    }
    
    if (!times.empty()) {
        double minTime = times[0];
        double maxTime = times[0];
        for (size_t i = 1; i < times.size(); i++) {
            if (times[i] < minTime) minTime = times[i];
            if (times[i] > maxTime) maxTime = times[i];
        }
        CkPrintf("Min time: %.2f μs\n", minTime);
        CkPrintf("Max time: %.2f μs\n", maxTime);
    }
    
    // Print percentiles
    double percentiles[] = {25.0, 50.0, 75.0, 90.0, 95.0, 99.0};
    for (size_t i = 0; i < sizeof(percentiles)/sizeof(percentiles[0]); i++) {
        CkPrintf("%.0fth percentile: %.2f μs\n", 
               percentiles[i], calculatePercentile(times, percentiles[i]));
    }
}

void GPUMemoryPool::reportStatsBySize(const char* description, const std::vector<OperationRecord>& records) {
    if (records.empty()) {
        CkPrintf("\n--- %s by Size Bucket ---\n", description);
        CkPrintf("No data found\n");
        return;
    }
    
    // Group into size buckets (powers of 10)
    std::map<size_t, std::vector<double> > sizeBuckets;
    
    for (size_t i = 0; i < records.size(); i++) {
        size_t size = records[i].size;
        double time = records[i].timeInMicroseconds;
        
        // Calculate bucket (10^n where n is number of digits - 1)
        size_t digits = 1;
        size_t temp = size;
        while (temp >= 10) {
            temp /= 10;
            digits++;
        }
        size_t bucket = 1;
        for (size_t j = 1; j < digits; j++) {
            bucket *= 10;
        }
        
        sizeBuckets[bucket].push_back(time);
    }
    
    CkPrintf("\n--- %s by Size Bucket ---\n", description);
    
    // Print stats for each bucket
    for (std::map<size_t, std::vector<double> >::iterator it = sizeBuckets.begin(); 
         it != sizeBuckets.end(); ++it) {
        std::vector<double>& bucketTimes = it->second;
        CkPrintf("Size ~%zu bytes: %zu operations, mean: %.2f μs, median: %.2f μs\n",
               it->first, bucketTimes.size(),
               calculateMean(bucketTimes),
               calculateMedian(bucketTimes));
    }
}

void GPUMemoryPool::printDetailedAnalytics() {
#if GPU_POOL_REPORTS == 0
    // Analytics reports disabled by compile-time flag
    return;
#endif

    if (!detailedAnalyticsEnabled) {
        CkPrintf("\n[GPUMemoryPool][PE %d] Detailed analytics not enabled. Enable with gpuPoolEnableDetailedStats(true)\n", 
               CmiMyPe());
        fflush(stdout);
        return;
    }
    
    CkPrintf("\n============ Detailed GPU Memory Analytics [PE %d] ============\n", CmiMyPe());
    
    // Report operation statistics
    reportOperationStats("New Allocations", newAllocationTimes);
    reportOperationStats("Block Reuse", reuseTimes);
    reportOperationStats("Adding to Free Pool", freePoolTimes);
    
    // Report statistics by size bucket
    reportStatsBySize("New Allocations", newAllocationTimes);
    reportStatsBySize("Block Reuse", reuseTimes);
    reportStatsBySize("Adding to Free Pool", freePoolTimes);
    
    // Compare reuse vs new allocation
    if (!newAllocationTimes.empty() && !reuseTimes.empty()) {
        std::vector<double> newAllocTimes, reuseTms;
        
        for (size_t i = 0; i < newAllocationTimes.size(); i++) {
            newAllocTimes.push_back(newAllocationTimes[i].timeInMicroseconds);
        }
        
        for (size_t i = 0; i < reuseTimes.size(); i++) {
            reuseTms.push_back(reuseTimes[i].timeInMicroseconds);
        }
        
        double newAllocMean = calculateMean(newAllocTimes);
        double reuseMean = calculateMean(reuseTms);
        
        CkPrintf("\nComparison: Is reuse faster than new allocation?\n");
        CkPrintf("New allocation mean time: %.2f μs\n", newAllocMean);
        CkPrintf("Block reuse mean time: %.2f μs\n", reuseMean);
        
        if (newAllocMean > 0) {
            double diffPercent = ((reuseMean - newAllocMean) / newAllocMean) * 100.0;
            CkPrintf("Difference: %.2f%% (%s for reuse)\n", 
                   diffPercent, (diffPercent > 0) ? "slower" : "faster");
        } else {
            CkPrintf("Cannot calculate percentage difference (division by zero)\n");
        }
    } else {
        CkPrintf("\nCannot compare reuse vs new allocation (insufficient data)\n");
    }
    
    CkPrintf("===========================================================\n");
    fflush(stdout);
} 
