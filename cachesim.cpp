#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <cmath>
#include <bitset>
#include <iomanip>
#include <list>

using namespace std;

typedef enum{
  HIT,
  MISS
} cacheStatus;

struct traceEntry{
char iord;	// Instruction or data miss (you can ignore)

char type;	// A data miss or write permission miss
		// A zero value means write permission miss and you can
		// treat these as hits. A non-zero value means a genuine L1
		// cache miss.

unsigned long long addr;	// Miss address (64 bits)

unsigned pc;	// Program counter of load/store instruction that missed (32 bits)
};

//create a mask st first a digit remain.
//11010101... -> 110000.... for a =2 if we apply the mask
u_int64_t createMask(u_int64_t a){
    u_int64_t  mask;
    //we have to use 1ull because standard shifting in cplusplus works for 32 bit only(?)
    mask = ((1ULL << a) - 1)<<(64-a);
    //std::bitset<64> b(mask);
    //std::cout<<"mask"<<" "<<b<<std::endl;
    return mask;
}


u_int64_t getFirstNbits(u_int64_t addr, u_int64_t a){
    u_int64_t m = createMask(a);
    //11010101... -> 110000.... for a =2 if we apply the mask
    u_int64_t value = addr & m;
    //std::bitset<64> b(value);
    //std::cout<<"value"<<" "<<b<<std::endl;
    //shift right to remove the trailing zeros
    //110000.... -> 11 for a =2 if we apply the mask
    return value>>(64-a);
}

u_int64_t getLastNbits(u_int64_t addr, u_int64_t a){
    u_int64_t m;
    m = (1ULL << a)-1;

    return addr & m;
}

struct cacheBlock{
  bool valid;
  u_int64_t tag;
};

struct cacheStats {
    u_int64_t reads;
    u_int64_t writes;
    u_int64_t readMisses;
    u_int64_t writeMisses;
};

struct config{
    u_int64_t level;
    u_int64_t blockSize;
    u_int64_t  setAssc;
    u_int64_t  totalSize;
    u_int64_t setCount;
    u_int64_t tagLen;
    u_int64_t indexLen;
    u_int64_t blockLen;
};

class Cache{
public:
  //the constructor
  Cache(config c){
    this->configParams = c;
    int setSize = (c.totalSize)/(c.setAssc);
    this->configParams.setCount = setSize/(c.blockSize);
    this->configParams.blockLen = ceil(log(configParams.blockSize)/log(2));
    this->configParams.indexLen = ceil(log(configParams.setCount)/log(2));
    this->configParams.tagLen = 64-(configParams.indexLen+configParams.blockLen);
    //cout<<"SS "<<setSize<<" Sc "<<this->configParams.setCount<<endl;
  //  cout<<"tag Length "<<configParams.tagLen <<" index Len "<<configParams.indexLen<<endl;
    data.resize(configParams.setCount, vector<cacheBlock>(c.setAssc));
    LRUList.resize(configParams.setCount);
  };

  void Init(){
    this->InvalidateFull();
    this->ResetStats();
  }

  //a few functions
  //sets all the valid bit to false
  void InvalidateFull(){
    //cout<<"Invalidating "<<data.size()<<" sets";
    for(int i=0;i<configParams.setCount;i++){
        for(int j=0;j<configParams.setAssc;j++){
              this->data[i][j].valid=false;
        }
    }
  };

  //invalidate a addr if it exists
  void Invalidate(u_int64_t addr){
    u_int64_t index = getIndex(addr);
    u_int64_t tag = getTag(addr);
    for (int j=0; j<configParams.setAssc; j++){
      if (data[index][j].tag==tag){
          data[index][j].valid= false;
          return;
      }
    }
    return;
  };

  void ResetStats(){
    stats = {0,0,0,0};
  };

  //return the way number in which the tag is stored in case of hit else -1
  int Find(u_int64_t addr){
    u_int64_t index = getIndex(addr);
    u_int64_t tag = getTag(addr);
    for (int j=0; j<configParams.setAssc; j++){
      //std::bitset<64> c(data[index][j].tag);
    //  cout<<"tag to be checked is"<<" "<<c<< endl;
      if (data[index][j].tag==tag){
        if (data[index][j].valid){
          return j;
        }
      }
    }
    return -1;
  };

//return 0 if no eviction else the evicted address.
  u_int64_t Load(u_int64_t addr){
    u_int64_t index = getIndex(addr);
    u_int64_t tag = getTag(addr);

    //Check for invalid blocks
    for (int j=0; j<configParams.setAssc; j++){
        if (!data[index][j].valid){
          data[index][j]= {true, tag};
          LRUList[index].push_front(j);
          return 0;
        }
    };

    //replacement
    int  wayNumberEvict = LRUList[index].back();
    u_int64_t evictedAddr = getAddr(index, data[index][wayNumberEvict].tag);
    LRUList[index].remove(wayNumberEvict);
    data[index][wayNumberEvict] = {true, tag};
    LRUList[index].push_front(wayNumberEvict);
    return evictedAddr;
  };

  cacheStatus Read(u_int64_t addr){
    stats.reads++;
    int r = Find(addr);
    if (r == -1){
      //miss
      stats.readMisses++;
      return MISS;
    }else{
      //hit
      //update the lru list of the set
      u_int64_t tag = getTag(addr);
      u_int64_t index = getIndex(addr);
      LRUList[index].remove(r);
      LRUList[index].push_front(r);
      return HIT;
    }
  };

  u_int64_t getTag(u_int64_t addr){
    //cout<<"getting the firts "<<configParams.tagLen<<" bits for tag"<<endl;
    u_int64_t tag = getFirstNbits(addr,configParams.tagLen);
    return tag;
  };

  u_int64_t getIndex(u_int64_t addr){
    //cout<<"getting the firts "<<(configParams.tagLen+configParams.indexLen)<<" bits for tag+index"<<endl;
    u_int64_t tagAndIndex = getFirstNbits(addr,(configParams.tagLen+configParams.indexLen));
    //std::bitset<64> y(tagAndIndex);
    //cout<<"tagandindex"<<" "<<y<< endl;
    //now we have tag+index combined. we just need to mask the tag out.
    return getLastNbits(tagAndIndex,configParams.indexLen);
  };

  u_int64_t getAddr( u_int64_t index, u_int64_t tag){
    int msb=0;
    //get the length of index say 2if index = 0..011
    u_int64_t indxcpy = index;
    //push until all the ones fall off to get the length
    while (indxcpy >>= 1) {
    msb++;
    }
    //std::cout<<"len of index is"<<msb+1<<std::endl;
    //shift the tag bits by lenofIndex if tag was 0...001 then after shift tag = 0..100 (len of index=2)
    u_int64_t shiftedTag = (tag << (msb+1));

    //concatenate the tag and index 0...100+0..011-> 0...111
    u_int64_t sum = shiftedTag | index;
    //count the length if this sum which is euqal to the length of shifted tag
    msb=0;
    while (shiftedTag >>= 1) {
    msb++;
    }
    //shift the concatenated digits to the left by (64-(lenof sum)) to get to the other end in 64 bit 000....111 -> 1110....00
    //return (sum << (64 - (msb+1)));
    return (sum<<(int)(this->configParams.blockLen));
  }

  void PrintConfig(){
    cout << left
       << setw(20) << "Level"<< setw(20)<<configParams.level<<endl;
    cout << left
          << setw(20) << "Block Size"<< setw(20)<<configParams.blockSize<<endl;
    cout << left
        << setw(20) << "Set-Associativity"<< setw(20)<<configParams.setAssc<<endl;
    cout << left
        << setw(20) << "Total Cache Size"<< setw(20)<<configParams.totalSize<<endl;
    cout << left
        << setw(20) << "Total Sets Count"<< setw(20)<<configParams.setCount<<endl;
    cout << left
        << setw(20) << "Tag Length"<< setw(20)<<configParams.tagLen<<endl;
    cout << left
        << setw(20) << "Index Length"<< setw(20)<<configParams.indexLen<<endl;
    cout << left
        << setw(20) << "Block Length"<< setw(20)<<configParams.blockLen<<endl;
  };

  void PrintStats(){
    cout<<"--[Level:"<<configParams.level<<"]--"<<endl;
    cout << left
       << setw(20) << "Reads"<< setw(20)<<stats.reads<<endl;
    cout << left
        << setw(20) << "Read Misses"<< setw(20)<<stats.readMisses<<endl;
    cout << left
        << setw(20) << "Read Hits"<< setw(20)<<stats.reads-stats.readMisses<<endl;
    cout << left
          << setw(20) << "Writes"<< setw(20)<<stats.writes<<endl;
    cout << left
          << setw(20) << "Write Misses"<< setw(20)<<stats.writeMisses<<endl;
    cout << left
          << setw(20) << "Write Hits"<< setw(20)<<stats.writes-stats.writeMisses<<endl;
  };

  void PrintIndex(u_int64_t addr){
    u_int64_t index = getIndex(addr);
    cout<<"--Printing Index:"<<index<<"--"<<endl;
    for (int j=0; j<configParams.setAssc; j++){
        if (data[index][j].valid){
          cout << left
             << setw(20) << "Way"<<j<<" tag:"<< setw(20)<<data[index][j].tag<<endl;
        }
        else{cout << left
           << setw(20) << "Way"<<j<<" tag:"<< setw(20)<<"invalid"<<endl;}
    }
  }

  //params of the cache
  config configParams;
  cacheStats stats;
  vector<vector<cacheBlock>> data;
  //for each set the lru order of each way number
  vector<std::list<int>> LRUList;
};

//Assumptions:
//if trace.type is zero it was a hit in L1 we wont count it as a miss. So no updation of L2orL3
int main(int argc, char** argv){
  config L2config = {2,64,8,(512*1024)}; //{level, blocksize, assoc, total cache size}
  Cache L2c(L2config);
  L2c.Init();
  cout<< "-----------CACHE INFO-----------"<<endl;
  L2c.PrintConfig();
  cout<<"---------"<<endl;
  //L2cache.PrintStats();

  config L3config = {3,64,16,(2*1024*1024)}; //{level, blocksize, assoc, total cache size}
  Cache L3c(L3config);
  L3c.Init();
  L3c.PrintConfig();

  int numtraces = atoi(argv[2]);
  char input_name[80];

  //inclusive
  for (int k=0; k<numtraces; k++) {
      sprintf(input_name, "%s_%d", argv[1], k);
      FILE * fp;
      fp = fopen(input_name, "rb");
      assert(fp != NULL);

      // Process the entry
      while (!feof(fp)) {
        struct traceEntry te;
        fread(&te.iord, sizeof(char), 1, fp);
        fread(&te.type, sizeof(char), 1, fp);
        fread(&te.addr, sizeof(unsigned long long), 1, fp);
        fread(&te.pc, sizeof(unsigned), 1, fp);

         //inclusive
         if(te.type){
           cacheStatus c2 = L2c.Read(te.addr);
           if(c2==MISS){
             cacheStatus c3 = L3c.Read(te.addr);
             if(c3==MISS){
               //level3 miss
               u_int64_t addrRem = L3c.Load(te.addr);
               if(addrRem){
                 //invalidate in l2 if it exists.
                 L2c.Invalidate(addrRem);
               };
             };
             L2c.Load(te.addr);
        }
         };
    }
      fclose(fp);
      printf("Done reading file %d!\n", k);
    }
  cout<< "-----------FILE: "<<argv[1]<<" INCLUSIVE-----------"<<endl;
  L2c.PrintStats();
  L3c.PrintStats();

  //reset the cache
  L2c.Init();
  L3c.Init();

  //exclusive
  for (int k=0; k<numtraces; k++) {
      sprintf(input_name, "%s_%d", argv[1], k);
      FILE * fp;
      fp = fopen(input_name, "rb");
      assert(fp != NULL);

      // Process the entry
      while (!feof(fp)) {
        struct traceEntry te;
        fread(&te.iord, sizeof(char), 1, fp);
        fread(&te.type, sizeof(char), 1, fp);
        fread(&te.addr, sizeof(unsigned long long), 1, fp);
        fread(&te.pc, sizeof(unsigned), 1, fp);

         //exclusive
         if(te.type){
           cacheStatus c2 = L2c.Read(te.addr);
           if(c2==MISS){
             cacheStatus c3 = L3c.Read(te.addr);
             if(c3==HIT){
               //level3 hit
               L3c.Invalidate(te.addr);
             };
             u_int64_t addrRem = L2c.Load(te.addr);
             if(addrRem){
               L3c.Load(te.addr);
             }
        }
         };
    }
      fclose(fp);
      printf("Done reading file %d!\n", k);
    }
    cout<< "-----------FILE: "<<argv[1]<<" EXCLUSIVE-----------"<<endl;
    L2c.PrintStats();
    L3c.PrintStats();

  //reset the cache
  L2c.Init();
  L3c.Init();

  //NINE
  for (int k=0; k<numtraces; k++) {
      sprintf(input_name, "%s_%d", argv[1], k);
      FILE * fp;
      fp = fopen(input_name, "rb");
      assert(fp != NULL);

      // Process the entry
      while (!feof(fp)) {
        struct traceEntry te;
        fread(&te.iord, sizeof(char), 1, fp);
        fread(&te.type, sizeof(char), 1, fp);
        fread(&te.addr, sizeof(unsigned long long), 1, fp);
        fread(&te.pc, sizeof(unsigned), 1, fp);

         //nine
         if(te.type){
           cacheStatus c2 = L2c.Read(te.addr);
           if(c2==MISS){
             cacheStatus c3 = L3c.Read(te.addr);
             if(c3==MISS){
               //l3 miss
               u_int64_t addrRem = L3c.Load(te.addr);
             }
             else{
               //l3hit
               L3c.Invalidate(te.addr);
             }
              //l3 miss or hit, in any case fill it in l2
             u_int64_t addrRem = L2c.Load(te.addr);
             if(addrRem){
               L3c.Load(te.addr);
             };
        };
         };
    }
      fclose(fp);
      printf("Done reading file %d!\n", k);
    }
    cout<< "-----------FILE: "<<argv[1]<<" NINE-----------"<<endl;
    L2c.PrintStats();
    L3c.PrintStats();
  return 0;
}
