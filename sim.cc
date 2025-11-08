#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <list>
#include <cmath>
#include <vector>
#include "sim.h"
#include <iostream>
#include <algorithm>
#include <limits.h>
#include <string.h>

using namespace std;

struct Cachedata
{
   bool valid;
   bool dirty;
   int LRU_count;
   uint32_t tag; 
};

struct Cacheblock                             
{
   int size;
   int block_size;
   int assoc;
   int sets;
   int tagbits;
   int indexbits;
   int offsetbits;
   std::vector<std::vector<Cachedata>> cachedata;
   Cacheblock *next;// next will give address stored in next (next variable value), *next will give the value at that address, &next will give address of variable next, &*next will give adress of the value that *next is pointing to
   Cacheblock *prev;
};

struct Streambuffer
{
   bool validsb;
   int LRU_count_sb;
   std::vector<uint32_t> sbelements;
};

struct Prefetch
{
   int numofbuffers;
   int numofelements;
   std::vector<Streambuffer> sb;
};

bool lrucompare(const Cachedata &a, const Cachedata &b)
{
   return (a.LRU_count > b.LRU_count);
}

Streambuffer create_stream_buffer(uint32_t addr, int m, int b)
{
   
   Streambuffer sb1;
   sb1.validsb = true;
   sb1.LRU_count_sb = 0;
   sb1.sbelements.resize(m);
   for(int j=0; j<m; j++)
   {
      sb1.sbelements[j]=addr + (j+1);
   }
   return sb1;
}

int main (int argc, char *argv[]) 
{
   FILE *fp;			// File pointer.
   char *trace_file;		// This variable holds the trace file name.
   cache_params_t params;	// Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;			// This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;		// This variable holds the request's address obtained from the trace.
				// The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }
    
   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);
   params.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file       = argv[8];

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }
    
   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("\n");

    ///////////////////////////////////////////////////////
    ////// Issue the request to the L1 cache instance here.
    ///////////////////////////////////////////////////////

      //L1 measurements
      int l1reads=0;
      int l1readmisses=0;
      int l1writes=0;
      int l1writemisses=0;
      int l1writeback=0;
      int memorytraffic=0;
      int prefetch=0;
      double l1missrate=0;   
      
      //L1 Cacheblock
      Cacheblock* l1 = new Cacheblock();
      l1->size=params.L1_SIZE;
      l1->assoc=params.L1_ASSOC;
      l1->block_size=params.BLOCKSIZE;
      l1->sets=params.L1_SIZE/(params.L1_ASSOC*params.BLOCKSIZE);
      l1->next=nullptr;
      l1->prev=nullptr;

      l1->indexbits=std::log2(l1->sets);
      l1->offsetbits=std::log2(params.BLOCKSIZE);
      l1->tagbits=32-l1->indexbits-l1->offsetbits;

      int count=0;      //global counter
   
      int l1hits=0;

      //L2 measurements
      int l2reads=0;
      int l2readmisses=0;
      int l2writes=0;
      int l2writemisses=0;
      int l2writeback=0;
      int prefetch2=0;
      double l2missrate=0; 

      Cacheblock* l2 = nullptr;
      //L2 Cacheblock
      if(params.L2_SIZE > 0)
      {   
         l2 = new Cacheblock();     
         l2->size=params.L2_SIZE;
         l2->assoc=params.L2_ASSOC;
         l2->block_size=params.BLOCKSIZE;
         l2->sets=params.L2_SIZE/(params.L2_ASSOC*params.BLOCKSIZE);
         l2->next=nullptr;
         l2->prev=nullptr;

         l2->indexbits=std::log2(l2->sets);
         l2->offsetbits=std::log2(params.BLOCKSIZE);
         l2->tagbits=32-l2->indexbits-l2->offsetbits;      

         //resizing the vector with the exact sets and assoc for L2
         l2->cachedata.resize(l2->sets, std::vector<Cachedata>(l2->assoc));

         for(int a=0; a<l2->sets; a++)
         {
            for(int s=0; s<l2->assoc; s++)
            {
               l2->cachedata[a][s].valid = false;
               l2->cachedata[a][s].dirty = false;
               l2->cachedata[a][s].LRU_count = 0;
            }
         }

      }

      
      //Prefetcher unit
      Prefetch* p = nullptr;
      if(params.PREF_N > 0)
      {
      p = new Prefetch();
      p->numofbuffers = params.PREF_N;
      p->numofelements = params.PREF_M;

      //resizing the number of stream buffers and number of elements in each stream buffer
      p->sb.resize(p->numofbuffers);

      for(int j=0; j<p->numofbuffers; j++)
      {
         p->sb[j].sbelements.resize(p->numofelements);
         p->sb[j].validsb = false;
         p->sb[j].LRU_count_sb=0;
      }
      }

      //resizing the vector with the exact sets and assoc for L1
      l1->cachedata.resize(l1->sets, std::vector<Cachedata>(l1->assoc));

      for(int a=0; a<l1->sets; a++)
      {
         for(int s=0; s<l1->assoc; s++)
         {
            l1->cachedata[a][s].valid = false;
            l1->cachedata[a][s].dirty = false;
            l1->cachedata[a][s].LRU_count = 0;
         }
      }



    // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) 
   {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
      if (rw == 'r')
      {
         //printf("r %x\n", addr);
         l1reads++;
      }
      else if (rw == 'w')
      {
         //printf("w %x\n", addr);
         l1writes++;
      }
      else 
      {
         printf("Error: Unknown request type %c.\n", rw);
	      exit(EXIT_FAILURE);
      }

      //extracting of bits FOR L1
      uint32_t indexl1, tagl1, offsetl1,indexmask,tagl1_final, l1victim;
      offsetl1 = ((1<<l1->offsetbits)-1);
      offsetl1 = offsetl1 & addr;


      indexmask = (l1->indexbits > 0) ? ((1u << l1->indexbits) - 1u) : 0u;
      indexl1 = (addr >> l1->offsetbits) & indexmask;
      tagl1 = (addr>>(l1->indexbits+ l1->offsetbits));
      bool hit = false;

      //extracting of bits FOR L2
      uint32_t indexl2, tagl2, offsetl2,indexmask2,tagl2_final, l2victim;

      // if(params.L2_SIZE > 0)
      // {
      //    offsetl2 = ((1<<l2->offsetbits)-1);
      //    offsetl2 = offsetl2 & addr;

      //    indexmask2 = (l2->indexbits > 0) ? ((1u << l2->indexbits) - 1u) : 0u;
      //    indexl2 = (addr >> l2->offsetbits) & indexmask2;
      //    tagl2 = (addr>>(l2->indexbits+ l2->offsetbits));
      // }

      //HIT scenario of L1
      
      for(int s=0; s<l1->assoc; s++)
      {
         if(tagl1 == l1->cachedata[indexl1][s].tag && l1->cachedata[indexl1][s].valid == true)
         {
            hit = true;
           // l1hits++;
            //STREAM BUFFER HIT
           /* if(params.PREF_N > 0)
            {
               for(int j=0; j<p->numofbuffers; j++)
               {            
               if(p->sb[j].validsb == true)
                  { 
                     for(int k=0; k<p->numofelements; k++)
                     {
                        if(p->sb[j].sbelements[k] == addr)
                        {                     
                           p->sb[j] = create_stream_buffer(addr, p->numofelements, l1->block_size);
                           p->sb[j].LRU_count_sb = count; 
                           prefetch += j+1;                    
                           break;
                        }
                     }
                     
                  }
               }
            }*/
            if (rw == 'w')
            {
               l1->cachedata[indexl1][s].dirty=true;
            }                    
            l1->cachedata[indexl1][s].LRU_count = ++count;
            break;
         }
      }

      //MISS scenario of L1
   
      if(!hit)
      {  
         //LRU Update 
         if (rw == 'w')
         {
            l1writemisses++;
         }
         else
         {
            l1readmisses++;
         }

         int minlrucount=l1->cachedata[indexl1][0].LRU_count;
         int minlrupos=0;   
         bool found = false;

         //Search for victim block in L1
         //Search for block with lowest LRU
         for(int s=0; s<l1->assoc; s++)
         {
            if(l1->cachedata[indexl1][s].LRU_count<minlrucount) 
            { 
               minlrucount = l1->cachedata[indexl1][s].LRU_count; 
               minlrupos = s; 
            }  
         } 

         // Check if the dirty bit of victim block is true for L1            
         if(l1->cachedata[indexl1][minlrupos].dirty==true)
         {
            //send value to L2 or Main memory, set Dirty to 0
            l1writeback++;
            l1->cachedata[indexl1][minlrupos].dirty = false;   
            if(params.L2_SIZE > 0) 
            {
                l2writes++; 
               
               //The tag is coming from L1, hence to update in L2, bit extraction must happen again
               {
                  l1victim = ((((l1->cachedata[indexl1][minlrupos].tag<<l1->indexbits) | indexl1) << (l1->offsetbits) ));   
                  offsetl2 = ((1u<<l2->offsetbits)-1u) & l1victim;

                  indexmask2 = (l2->indexbits > 0) ? ((1u << l2->indexbits) - 1u) : 0u;
                  indexl2 = (l1victim >> l2->offsetbits) & indexmask2;
                  tagl2 = (l1victim>>(l2->indexbits+ l2->offsetbits));
               }
               int minlrucount2=l2->cachedata[indexl2][0].LRU_count;
               int minlrupos2=0;
               //Check if it is an L2 hit
               bool l2_wm_check = false;
               for(int t=0; t<l2->assoc; t++)
               {                  
                  if(l2->cachedata[indexl2][t].valid && l2->cachedata[indexl2][t].tag == tagl2)
                  {                        
                     l2_wm_check = true;
                     l2->cachedata[indexl2][t].dirty = true;
                     l2->cachedata[indexl2][t].LRU_count = ++count;
                     break;
                  }
               } 
               // L2 miss 
               
               if( !l2_wm_check)
               {
                  l2writemisses++;
                  //Search for L2 victim block
                  for(int s=0; s<l2->assoc; s++)
                  {
                     if(l2->cachedata[indexl2][s].LRU_count<minlrucount2) 
                     { 
                        minlrucount2 = l2->cachedata[indexl2][s].LRU_count; 
                        minlrupos2 = s; 
                     }  
                  } 

                  //Is Victim block bit dirty?
                  if(l2->cachedata[indexl2][minlrupos2].dirty == true && l2->cachedata[indexl2][minlrupos2].valid == true )
                  {
                     l2writeback++;
                     l2->cachedata[indexl2][minlrupos2].dirty = false;
                  }
                  l2->cachedata[indexl2][minlrupos2].dirty = true;
                  l2->cachedata[indexl2][minlrupos2].LRU_count = ++count;
                  l2->cachedata[indexl2][minlrupos2].valid = 1;
                  l2->cachedata[indexl2][minlrupos2].tag = tagl2;
               }
            } 
         }
         //Dirty bit of L1 victim block is 0

         if(l1->cachedata[indexl1][minlrupos].dirty==false)
         {
            if(params.L2_SIZE > 0)
            {
               l2reads++;

               //The tag is not in L1, hence to update in L2, bit extraction must happen again
               {
                  offsetl2 = ((1<<l2->offsetbits)-1)&addr;

                  indexmask2 = (l2->indexbits > 0) ? ((1u << l2->indexbits) - 1u) : 0u;
                  indexl2 = (addr >> l2->offsetbits) & indexmask2;
                  tagl2 = (addr>>(l2->indexbits+ l2->offsetbits));
               }
               int minlrucount2=l2->cachedata[indexl2][0].LRU_count;
               int minlrupos2=0;
              //Check if it is an L2 hit
               bool l2_wm_check = false;
               for(int t=0; t<l2->assoc; t++)
               {
                  if(l2->cachedata[indexl2][t].valid == true && l2->cachedata[indexl2][t].tag == tagl2)
                  {                        
                     l2_wm_check = true;
                     l2->cachedata[indexl2][t].LRU_count = ++count;
                     break;
                  }
               }
                //L2 miss with L1 victim block D=0
               if(!l2_wm_check)
               {
                  l2readmisses++;

                  //Search for victim block with lowest lru in L2
                  for(int s=0; s<l2->assoc; s++)
                  {
                     if(l2->cachedata[indexl2][s].LRU_count<minlrucount2) 
                     { 
                        minlrucount2 = l2->cachedata[indexl2][s].LRU_count; 
                        minlrupos2 = s; 
                     }  
                  } 
                  //Check if the block is dirty or not
                  if(l2->cachedata[indexl2][minlrupos2].dirty == true && l2->cachedata[indexl2][minlrupos2].valid == true)
                  {
                     l2writeback++;
                     l2->cachedata[indexl2][minlrupos2].dirty = false;
                  }
                     l2->cachedata[indexl2][minlrupos2].LRU_count = ++count;
                     l2->cachedata[indexl2][minlrupos2].valid = 1; 
                     l2->cachedata[indexl2][minlrupos2].tag = tagl2;
                     l2->cachedata[indexl2][minlrupos2].dirty = false; 
               }
            }
         }

         //Replace it with new tag
         l1->cachedata[indexl1][minlrupos].tag = tagl1;
         l1->cachedata[indexl1][minlrupos].valid = true;
         if (rw == 'w')
         {
            l1->cachedata[indexl1][minlrupos].dirty=true;
         }                    
         l1->cachedata[indexl1][minlrupos].LRU_count = ++count;
      }
      //count++;
   }                 

         //CACHE MISS STREAM BUFFER HIT
         // if(params.PREF_N > 0)
         // {
         // bool sb_addr_check = false;
         // for(int j=0; j<p->numofbuffers; j++)
         // {            
         //    if(p->sb[j].validsb == true)
         //    { 
         //       for(int k=0; k<p->numofelements; k++)
         //       {
         //          if(p->sb[j].sbelements[k] == addr)
         //          {                     
         //             //replace_element_in_buffer(p->sb[j]);
         //             p->sb[j] = create_stream_buffer(addr, p->numofelements, l1->block_size);
         //             p->sb[j].LRU_count_sb = count;
         //             sb_addr_check = true;
         //             if (rw == 'w')
         //             {
         //                l1writemisses--;
         //             }
         //             else
         //             {
         //                l1readmisses--;
         //             }
         //             break;
         //          }

         //       }
         //       if(sb_addr_check) break;
         //    }
         // }
         // bool empty_sb_check=false;
         // int min_lru_sb=(p && p->numofbuffers>0)? p->sb[0].LRU_count_sb:0;
         
         // CACHE MISS STREAM BUFFER MISS
         // if(!sb_addr_check)
         // {
         //    //prefetch++; //increment prefetch counter
         //    //CHECK FOR EMPTY BUFFER
         //    for(int j=0; j<p->numofbuffers; j++)
         //    {            
         //       if(p->sb[j].validsb == false)
         //       {
         //          //fill X+M address
         //          //replace_element_in_buffer(p->sb[j]);
         //          p->sb[j] = create_stream_buffer(addr, p->numofelements, l1->block_size);
         //          p->sb[j].LRU_count_sb = count;    
         //          empty_sb_check = true;
         //          prefetch += j+1; 
         //          break;                         
         //       }
         //    }

            //NO EMPTY BUFFER, SEARCH FOR LEAST LRU COUNT
            // int min_lru_sb_index = 0;
            // if(!empty_sb_check)
            // {
               //prefetch++; //increment prefetch counter
               // for(int j=0; j<p->numofbuffers; j++)
               // { 
               //    if(p->sb[j].LRU_count_sb<min_lru_sb) 
               //    { 
               //       min_lru_sb = p->sb[j].LRU_count_sb;
               //       min_lru_sb_index = j;
               //    }  
               // }

               //replace_element_in_buffer(p->sb[j]);
         //       p->sb[min_lru_sb_index] = create_stream_buffer(addr, p->numofelements, l1->block_size);
         //       p->sb[min_lru_sb_index].LRU_count_sb = count; 
         //       prefetch += min_lru_sb_index+1;  
         //    }        
         // }
         // }
       
        
         
         

      
   
    
   //L1 miss rate = MRL1 = (L1 read misses + L1 write misses)/(L1 reads + L1 writes) 
      l1missrate = (double(l1readmisses + l1writemisses)/double(l1reads + l1writes));
   
      //Sort elements of set in the order of MRU -> LRU
      //print contents of cache
      cout<<"===== L1 contents ====="<<endl;
      for(int a=0; a<l1->sets; a++)
      {
         vector<Cachedata> sortedblock;
         for(int s=0; s<l1->assoc; s++)
         {
            if(l1->cachedata[a][s].valid == true)
            {  
               sortedblock.push_back(l1->cachedata[a][s]);
            }
         }
         
         if(sortedblock.empty()) continue;

         sort(sortedblock.begin(), sortedblock.end(), lrucompare);

         if(a < 10) cout<<"set      "<<a<<":   ";
         else cout<<"set     "<<a<<":   ";

         for(int m = 0; m<sortedblock.size(); m++)
         {
            cout<<hex<<sortedblock[m].tag<<dec;
         
         
            if(sortedblock[m].dirty == true)
            {
               cout<<" D ";
            }
            else
            {
               cout<<"  ";
            }

         }   
         if(a != l1->sets - 1) cout<<endl;
            
      }
      if(params.L2_SIZE>0)
      {
      cout<<endl;
      cout<<endl;
      cout<<"===== L2 contents ====="<<endl;
      for(int a=0; a<l2->sets; a++)
      {
         vector<Cachedata> sortedblock2;
         for(int s=0; s<l2->assoc; s++)
         {
            if(l2->cachedata[a][s].valid == true)
            {  
               sortedblock2.push_back(l2->cachedata[a][s]);
            }
         }
         
         if(sortedblock2.empty()) continue;

         sort(sortedblock2.begin(), sortedblock2.end(), lrucompare);

         if(a < 10) cout<<"set      "<<a<<":   ";
         else cout<<"set     "<<a<<":   ";

         for(int m = 0; m<sortedblock2.size(); m++)
         {
            cout<<hex<<sortedblock2[m].tag<<dec;
         
         
            if(sortedblock2[m].dirty == true)
            {
               cout<<" D ";
            }
            else
            {
               cout<<"  ";
            }

         }   
         if(a != l2->sets - 1) cout<<endl;            
      }
      }


      if(params.PREF_N > 0)
      {
         cout<<endl;
         cout<<"===== Stream Buffer(s) contents ====="<<endl;
         for(int j = 0; j < params.PREF_N; j++)
         {
            for(int k = 0; k < params.PREF_M; k++ )
            {
               cout<<hex<<p->sb[j].sbelements[k]<<" "<<dec;
            }
            cout<<endl;
         }
      }
      else
      {
         cout<<endl;
      }
      if(params.L2_SIZE>0)
      {
         memorytraffic = l2readmisses + l2writemisses + l2writeback+ prefetch;
         l2missrate = (double(l2readmisses)/double(l1readmisses + l1writemisses));
      }
      else
      {
         memorytraffic = l1readmisses + l1writemisses + l1writeback+ prefetch;
      }
      cout<<endl;
      cout<<"===== Measurements ====="<<endl;
      cout<<"a. L1 reads:                   "<<l1reads<<endl;
      cout<<"b. L1 read misses:             "<<l1readmisses<<endl;
      cout<<"c. L1 writes:                  "<<l1writes<<endl;
      cout<<"d. L1 write misses:            "<<l1writemisses<<endl;
      printf("e. L1 miss rate:               %.4lf\n", l1missrate);
      cout<<"f. L1 writebacks:              "<<l1writeback<<endl;
      cout<<"g. L1 prefetches:              "<<prefetch<<endl;
      cout<<"h. L2 reads (demand):          "<<l2reads<<endl;
      cout<<"i. L2 read misses (demand):    "<<l2readmisses<<endl;
      cout<<"j. L2 reads (prefetch):        "<<"0"<<endl;
      cout<<"k. L2 read misses (prefetch):  "<<"0"<<endl;
      cout<<"l. L2 writes:                  "<<l2writes<<endl;
      cout<<"m. L2 write misses:            "<<l2writemisses<<endl;
      printf("n. L2 miss rate:               %.4lf\n", l2missrate);
      cout<<"o. L2 writebacks:              "<<l2writeback<<endl;
      cout<<"p. L2 prefetches:              "<<"0"<<endl;
      cout<<"q. memory traffic:             "<<memorytraffic<<endl;
    return(0);
}