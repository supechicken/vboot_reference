/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_CALL_TRACE_H
#define VBOOT_REFERENCE_CALL_TRACE_H

/*
 * Maximum number of calls to be traced. If calls exceed this value, the older
 * records are overwritten.
 */
#define VB_NUM_CALL_RECORD	16

/* If this is set, success returns are also recorded */
#define VB_TRACE_SUCCESS	0

struct call_record {
	/* Function name */
	const char *func;
	/* Returned code */
	int err;
};

/* Defines storage in memory where calls are recorded */
struct call_trace {
	unsigned int idx;
	struct call_record rec[VB_NUM_CALL_RECORD];
};

/**
 * Initialize call trace
 *
 * @param ct: Pointer to call trace
 */
void vb_init_call_trace(struct call_trace *ct);

/**
 * Record return code
 *
 * @param func: Function name in string
 * @param err: Return code to be recorded
 * @return: Return code passed through
 */
int vb_push_return_code(const char *func, int err);

/**
 * Dump recorded calls
 */
void vb_dump_call_trace(void);

/*
 * Note explicitly that x is only evaluated once in each macro. So this is
 * side-effect-free even if x is an expression which has side effects (such as
 * incrementing a global variable)
 */
#ifdef VB_TRACE_CALL
/*
 * Record a function and return value. Ideally, every single return statement
 * in a function should wrap a return value with this macro. It does not include
 * 'return' so that return statements still look normal. That is, we consider
 * 	return TRACE_RETURN(VB_ERROR_XXX)
 * looks better than
 * 	TRACE_RETURN(VB_ERROR_XXX)
 *
 * Speed overhead is as much as writing two values in memory at every return.
 * Space overhead is as munch as space needed to store function names.
 *
 * Alternatively, we considered storing a function ID instead of function name
 * to reduce space overhead. First, it forces developers to manage the
 * function ID table. Second since there is no built-in macro which is replaced
 * by a function name, TRACE_RETURN also needs to take function ID. Third, It
 * also makes trace dump unreadable because function IDs have to be converted.
 *
 * Based on these observations, we chose this form of TRACE_RETURN.
 */
#define TRACE_RETURN(err) vb_push_return_code(__func__, err)
/*
 * Record return code manually. For example, a return value from a call to an
 * external function can be recorded by using this macro.
 */
#define PUSH_RETURN_CODE(func, err) vb_push_return_code(func, err)
#else
#define TRACE_RETURN(err) (err)
#define PUSH_RETURN_CODE(func, err)
#endif

/**
 * Record return value and pass it throught to the caller
 */
#define RETURN_ON_ERROR(function_call) do {				\
	int rv = (function_call);					\
	if (rv)								\
		return TRACE_RETURN(rv);				\
} while (0)

#endif
