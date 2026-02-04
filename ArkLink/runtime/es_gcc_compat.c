



#ifdef _WIN64

typedef unsigned long long uint64_t;
typedef long long int64_t;


int64_t __divdi3(int64_t a, int64_t b) {
    return a / b;
}


uint64_t __udivdi3(uint64_t a, uint64_t b) {
    return a / b;
}


int64_t __moddi3(int64_t a, int64_t b) {
    return a % b;
}


uint64_t __umoddi3(uint64_t a, uint64_t b) {
    return a % b;
}


int64_t __muldi3(int64_t a, int64_t b) {
    return a * b;
}

#endif 





double __floatundidf(uint64_t a) {
    return (double)a;
}

uint64_t __fixunsdfdi(double a) {
    return (uint64_t)a;
}

int64_t __fixdfdi(double a) {
    return (int64_t)a;
}



void __main(void) {
    
    
}
