#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int _tc=0, _tp=0, _tf=0;
#define ASSERT(cond, msg) do { _tc++; \
    if(cond){_tp++;printf("  PASS  %s\n",msg);} \
    else{_tf++;printf("  FAIL  %s  (line %d)\n",msg,__LINE__);} \
} while(0)
#define ASSERT_EQ(a,b,msg)  ASSERT((a)==(b),msg)
#define ASSERT_NEQ(a,b,msg) ASSERT((a)!=(b),msg)
#define ASSERT_GT(a,b,msg)  ASSERT((a)>(b),msg)
#define ASSERT_LT(a,b,msg)  ASSERT((a)<(b),msg)
#define ASSERT_NULL(p,msg)  ASSERT((p)==NULL,msg)
#define ASSERT_NOTNULL(p,msg) ASSERT((p)!=NULL,msg)
#define ASSERT_STR(a,b,msg) ASSERT(strcmp((a),(b))==0,msg)
#define ASSERT_NEAR(a,b,eps,msg) ASSERT(fabs((a)-(b))<(eps),msg)
#define SUITE(name) printf("\n=== %s ===\n",name)
#define TEST_RESULTS() do { \
    printf("\nResults: %d/%d passed",_tp,_tc); \
    if(_tf) printf(", %d FAILED",_tf); \
    printf("\n"); \
    return (_tf>0)?1:0; \
} while(0)
