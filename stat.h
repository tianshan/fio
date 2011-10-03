#ifndef FIO_STAT_H
#define FIO_STAT_H

struct group_run_stats {
	uint64_t max_run[2], min_run[2];
	uint64_t max_bw[2], min_bw[2];
	uint64_t io_kb[2];
	uint64_t agg[2];
	uint32_t kb_base;
	uint32_t groupid;
};

/*
 * How many depth levels to log
 */
#define FIO_IO_U_MAP_NR	7
#define FIO_IO_U_LAT_U_NR 10
#define FIO_IO_U_LAT_M_NR 12

/*
 * Aggregate clat samples to report percentile(s) of them.
 *
 * EXECUTIVE SUMMARY
 *
 * FIO_IO_U_PLAT_BITS determines the maximum statistical error on the
 * value of resulting percentiles. The error will be approximately
 * 1/2^(FIO_IO_U_PLAT_BITS+1) of the value.
 *
 * FIO_IO_U_PLAT_GROUP_NR and FIO_IO_U_PLAT_BITS determine the maximum
 * range being tracked for latency samples. The maximum value tracked
 * accurately will be 2^(GROUP_NR + PLAT_BITS -1) microseconds.
 *
 * FIO_IO_U_PLAT_GROUP_NR and FIO_IO_U_PLAT_BITS determine the memory
 * requirement of storing those aggregate counts. The memory used will
 * be (FIO_IO_U_PLAT_GROUP_NR * 2^FIO_IO_U_PLAT_BITS) * sizeof(int)
 * bytes.
 *
 * FIO_IO_U_PLAT_NR is the total number of buckets.
 *
 * DETAILS
 *
 * Suppose the clat varies from 0 to 999 (usec), the straightforward
 * method is to keep an array of (999 + 1) buckets, in which a counter
 * keeps the count of samples which fall in the bucket, e.g.,
 * {[0],[1],...,[999]}. However this consumes a huge amount of space,
 * and can be avoided if an approximation is acceptable.
 *
 * One such method is to let the range of the bucket to be greater
 * than one. This method has low accuracy when the value is small. For
 * example, let the buckets be {[0,99],[100,199],...,[900,999]}, and
 * the represented value of each bucket be the mean of the range. Then
 * a value 0 has an round-off error of 49.5. To improve on this, we
 * use buckets with non-uniform ranges, while bounding the error of
 * each bucket within a ratio of the sample value. A simple example
 * would be when error_bound = 0.005, buckets are {
 * {[0],[1],...,[99]}, {[100,101],[102,103],...,[198,199]},..,
 * {[900,909],[910,919]...}  }. The total range is partitioned into
 * groups with different ranges, then buckets with uniform ranges. An
 * upper bound of the error is (range_of_bucket/2)/value_of_bucket
 *
 * For better efficiency, we implement this using base two. We group
 * samples by their Most Significant Bit (MSB), extract the next M bit
 * of them as an index within the group, and discard the rest of the
 * bits.
 *
 * E.g., assume a sample 'x' whose MSB is bit n (starting from bit 0),
 * and use M bit for indexing
 *
 *        | n |    M bits   | bit (n-M-1) ... bit 0 |
 *
 * Because x is at least 2^n, and bit 0 to bit (n-M-1) is at most
 * (2^(n-M) - 1), discarding bit 0 to (n-M-1) makes the round-off
 * error
 *
 *           2^(n-M)-1    2^(n-M)    1
 *      e <= --------- <= ------- = ---
 *             2^n          2^n     2^M
 *
 * Furthermore, we use "mean" of the range to represent the bucket,
 * the error e can be lowered by half to 1 / 2^(M+1). By using M bits
 * as the index, each group must contains 2^M buckets.
 *
 * E.g. Let M (FIO_IO_U_PLAT_BITS) be 6
 *      Error bound is 1/2^(6+1) = 0.0078125 (< 1%)
 *
 *	Group	MSB	#discarded	range of		#buckets
 *			error_bits	value
 *	----------------------------------------------------------------
 *	0*	0~5	0		[0,63]			64
 *	1*	6	0		[64,127]		64
 *	2	7	1		[128,255]		64
 *	3	8	2		[256,511]		64
 *	4	9	3		[512,1023]		64
 *	...	...	...		[...,...]		...
 *	18	23	17		[8838608,+inf]**	64
 *
 *  * Special cases: when n < (M-1) or when n == (M-1), in both cases,
 *    the value cannot be rounded off. Use all bits of the sample as
 *    index.
 *
 *  ** If a sample's MSB is greater than 23, it will be counted as 23.
 */

#define FIO_IO_U_PLAT_BITS 6
#define FIO_IO_U_PLAT_VAL (1 << FIO_IO_U_PLAT_BITS)
#define FIO_IO_U_PLAT_GROUP_NR 19
#define FIO_IO_U_PLAT_NR (FIO_IO_U_PLAT_GROUP_NR * FIO_IO_U_PLAT_VAL)
#define FIO_IO_U_LIST_MAX_LEN 20 /* The size of the default and user-specified
					list of percentiles */

#define MAX_PATTERN_SIZE	512
#define FIO_JOBNAME_SIZE	128
#define FIO_VERROR_SIZE		128

struct thread_stat {
	char name[FIO_JOBNAME_SIZE];
	char verror[FIO_VERROR_SIZE];
	int32_t error;
	int32_t groupid;
	uint32_t pid;
	char description[FIO_JOBNAME_SIZE];
	uint32_t members;

	/*
	 * bandwidth and latency stats
	 */
	struct io_stat clat_stat[2];		/* completion latency */
	struct io_stat slat_stat[2];		/* submission latency */
	struct io_stat lat_stat[2];		/* total latency */
	struct io_stat bw_stat[2];		/* bandwidth stats */

	/*
	 * fio system usage accounting
	 */
	uint64_t usr_time;
	uint64_t sys_time;
	uint64_t ctx;
	uint64_t minf, majf;

	/*
	 * IO depth and latency stats
	 */
	uint64_t clat_percentiles;
	double *percentile_list;

	uint32_t io_u_map[FIO_IO_U_MAP_NR];
	uint32_t io_u_submit[FIO_IO_U_MAP_NR];
	uint32_t io_u_complete[FIO_IO_U_MAP_NR];
	uint32_t io_u_lat_u[FIO_IO_U_LAT_U_NR];
	uint32_t io_u_lat_m[FIO_IO_U_LAT_M_NR];
	uint32_t io_u_plat[2][FIO_IO_U_PLAT_NR];
	uint64_t total_io_u[3];
	uint64_t short_io_u[3];
	uint64_t total_submit;
	uint64_t total_complete;

	uint64_t io_bytes[2];
	uint64_t runtime[2];
	uint64_t total_run_time;

	/*
	 * IO Error related stats
	 */
	uint16_t continue_on_error;
	uint64_t total_err_count;
	int32_t first_error;

	uint32_t kb_base;
};

extern void show_thread_status(struct thread_stat *ts, struct group_run_stats *rs);
extern void show_group_stats(struct group_run_stats *rs);


#endif