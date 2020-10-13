#include <bitset>

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
