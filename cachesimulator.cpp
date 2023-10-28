#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

struct CacheBlock {
    unsigned int tag = 0;
    bool valid = false;
    bool dirty = false;
};

class Set {
public:
    std::vector<CacheBlock> ways; // Renamed from blocks to ways
    int evictionCounter = 0;

    Set() {}
    Set(int associativity) : ways(associativity) {}

    int findBlock(unsigned int tag) {
        for (int i = 0; i < ways.size(); i++) {
            if (ways[i].valid && ways[i].tag == tag) {
                return i;
            }
        }
        return -1;
    }

    int findEmptyBlock() {
        for (int i = 0; i < ways.size(); i++) {
            if (!ways[i].valid) {
                return i;
            }
        }
        return -1;
    }

    int evict() {
        int evictedBlock = evictionCounter;
        evictionCounter = (evictionCounter + 1) % ways.size();
        return evictedBlock;
    }

    void insertBlock(unsigned int tag) {
        int emptyBlock = findEmptyBlock();
        if (emptyBlock != -1) {
            ways[emptyBlock].tag = tag;
            ways[emptyBlock].valid = true;
            ways[emptyBlock].dirty = false; // Assuming block is not dirty when newly inserted
        } else {
            int blockToEvict = evict();
            ways[blockToEvict].tag = tag;
            ways[blockToEvict].valid = true;
            ways[blockToEvict].dirty = false; // Assuming block is not dirty when newly inserted
        }
    }
};


class Cache {
public:
    std::vector<Set> sets;
    int blockSize;
    int numSets;
    int associativity;
    int indexBits;
    int blockOffsetBits;
    unsigned int indexMask; // mask for extracting index from address

    Cache(int blockSize, int associativity, int totalSize)
        : blockSize(blockSize), associativity(associativity) {
        // Calculate the number of sets based on total cache size, block size, and associativity
        numSets = totalSize * 1024 / (blockSize * associativity);
        sets.resize(numSets);

        // Calculate bit sizes for index and block offset
        blockOffsetBits = log2(blockSize);
        indexBits = log2(numSets);

        // Calculate the index mask based on the number of index bits
        indexMask = (1 << indexBits) - 1;
    }

    int read(unsigned int address);
    int write(unsigned int address);
    int evictAndMove(unsigned int address, Cache &lowerLevelCache);
};

int Cache::read(unsigned int address) {
    unsigned int index = (address >> blockOffsetBits) & ((1 << indexBits) - 1);
    unsigned int tag = address >> (blockOffsetBits + indexBits);
    Set &set = sets[index];

    if (set.findBlock(tag) != -1) {
        return 1;  // Read Hit
    } else {
        set.insertBlock(tag);
        return 2;  // Read Miss
    }
}

int Cache::write(unsigned int address) {
    unsigned int index = (address >> blockOffsetBits) & indexMask;
    unsigned int tag = address >> (indexBits + blockOffsetBits);

    for (int i = 0; i < associativity; ++i) {
        if (sets[index].ways[i].valid && sets[index].ways[i].tag == tag) {
            // Write Hit
            sets[index].ways[i].dirty = true;  // Set dirty bit
            return 3;
        }
    }
    // Write Miss
    return 4;
}

int Cache::evictAndMove(unsigned int address, Cache &lowerLevelCache) {
    unsigned int index = (address >> blockOffsetBits) & ((1 << indexBits) - 1);
    unsigned int tag = address >> (blockOffsetBits + indexBits);
    Set &set = sets[index];

    int evictBlock = set.evict();

    // If the block is dirty, move it to the lower level cache.
    if (set.ways[evictBlock].dirty) {  // Renamed set.blocks to set.ways
        lowerLevelCache.write(address);
    }

    // Invalidate the block in the current cache.
    set.ways[evictBlock].valid = false;  // Renamed set.blocks to set.ways
    set.ways[evictBlock].dirty = false;  // Renamed set.blocks to set.ways

    return evictBlock;
}

struct CacheConfig {
    int L1BlockSize;
    int L1Associativity;
    int L1Size;

    int L2BlockSize;
    int L2Associativity;
    int L2Size;
};

void processTrace(const CacheConfig &config) {
    std::ifstream traceFile("trace.txt");
    std::ofstream outputFile("trace.txt.out");
    Cache L1(config.L1BlockSize, config.L1Associativity, config.L1Size);
    Cache L2(config.L2BlockSize, config.L2Associativity, config.L2Size);

    char accessType;
    unsigned int address;
    while (traceFile >> accessType >> std::hex >> address) {
        int L1Result = 0, L2Result = 0, memWrite = 5;

        if (accessType == 'R') {
            L1Result = L1.read(address);
            if (L1Result == 2) {  // Read miss in L1
                L2Result = L2.read(address);
                if (L2Result == 1) {  // Read hit in L2
                    L1.evictAndMove(address, L2);
                    L1.read(address);
                    L2Result = 0;  // Invalidate block in L2
                } else if (L2Result == 2) {
                    memWrite = 5;  // No write to memory since it's a read.
                }
            }
        } else if (accessType == 'W') {  // Fixed the misplaced if-check for 'W' here
            L1Result = L1.write(address);
            if (L1Result == 4) {  // Write miss in L1
                L2Result = L2.write(address);
                if (L2Result == 4) {  // Write miss in L2 as well
                    memWrite = 6;  // Direct write to main memory
                } else if (L2Result == 3) {  // Write hit in L2
                    memWrite = 5;  // No write to main memory, just mark L2 block dirty
                }
            } else if (L1Result == 3) {  // Write hit in L1
                memWrite = 5;  // No write to main memory, just mark L1 block dirty
            }
        }

        outputFile << L1Result << " " << L2Result << " " << memWrite << std::endl;
    }

    traceFile.close();
    outputFile.close();
}

int main() {
    CacheConfig config;
    std::ifstream configFile("cacheconfig.txt");
    std::string label;

    configFile >> label;  // Read and discard the "L1:" label
    configFile >> config.L1BlockSize >> config.L1Associativity >> config.L1Size;

    configFile >> label;  // Read and discard the "L2:" label
    configFile >> config.L2BlockSize >> config.L2Associativity >> config.L2Size;

    configFile.close();


    processTrace(config);
    return 0;
}
