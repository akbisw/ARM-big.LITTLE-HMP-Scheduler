/* Show per core CPU utilization of the system 
 * This is a part of the post http://phoxis.org/2013/09/05/finding-overall-and-per-core-cpu-utilization
 */
#include <stdio.h>
#include <unistd.h>
#include <time.h> 
#define BUF_MAX 1024
#define MAX_CPU 128

 
int read_fields (FILE *fp, unsigned long long int *fields)
{
  int retval;
  char buffer[BUF_MAX];
 
  /*  Get string from stream */
  //Reads characters from stream and stores them as a C string into str until (num-1) characters have been read or either a newline or the end-of-file is reached, whichever happens first.
  if (!fgets (buffer, BUF_MAX, fp))
  { perror ("Error"); }
  /* line starts with c and a string. This is to handle cpu, cpu[0-9]+ */
  // * skips the match. %Lu matches long unsigned
  // { "user","nice","system","idle","iowait","irq","softirq","steal","guest","guest_nice" }
  retval = sscanf (buffer, "c%*s %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
                            &fields[0], 
                            &fields[1], 
                            &fields[2], 
                            &fields[3], 
                            &fields[4], 
                            &fields[5], 
                            &fields[6], 
                            &fields[7], 
                            &fields[8], 
                            &fields[9]);
  if (retval == 0)
  { return -1; }
  if (retval < 4) /* Atleast 4 fields is to be read */
  {
    fprintf (stderr, "Error reading /proc/stat cpu field\n");
    return 0;
  }
  return 1;
}
// Simply writes metrics to a csv file. 
void writeCSV(int cpu_num, float percent, char* metric_type)
{
	time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char file_name[14];
        sprintf(file_name, "cpu%d_%s.csv", cpu_num, metric_type);
        FILE *f_ptr = fopen(file_name, "a");
        fprintf(f_ptr, "%d-%d-%d %d:%d:%d,%3.2lf\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, percent);
        fclose(f_ptr);
}


 
int main (void)
{
  FILE *fp;
  unsigned long long int fields[10], total_tick[MAX_CPU], total_tick_old[MAX_CPU], idle[MAX_CPU], idle_old[MAX_CPU], del_total_tick[MAX_CPU], del_idle[MAX_CPU];
  int update_cycle = 0, i, cpus = 0, count;
  double percent_usage;

  fp = fopen ("/proc/stat", "r");
  if (fp == NULL)
  {
    perror ("Error");
  }
 
  /* Parse the /proc/stat file until you find cpu */
  // This loops until it reads all 9 cpu lines 
  // Goal is to count how many cpus the system has
  while (read_fields (fp, fields) != -1)
  {
    for (i=0, total_tick[cpus] = 0; i<10; i++)
    { total_tick[cpus] += fields[i]; }
    idle[cpus] = fields[3]; /* idle ticks index */
    cpus++; 
  }
  
  while (1)
  {
    /* Sleep for 1 second */
    sleep (1);
    /* Seek to the beginning of the /proc/stat file for a BRAND NEW CPU READING */
    fseek (fp, 0, SEEK_SET);
    /* Flush the file pointer to update the new cpu values stored by kernel */
    fflush (fp);
    // keep count of how many time cpu stats has been updated
    printf ("[Update cycle %d]\n", update_cycle); 

    for (count = 0; count < cpus; count++)
    {
      // Store the old field counters
      total_tick_old[count] = total_tick[count];
      idle_old[count] = idle[count];
      // read new counter values
      if (!read_fields (fp, fields))
      { return 0; }
      // update the current ticks
      for (i=0, total_tick[count] = 0; i<10; i++)
      { total_tick[count] += fields[i]; }
      idle[count] = fields[3];
      // Calculate the delta between the old counters and the current ones
      del_total_tick[count] = total_tick[count] - total_tick_old[count];
      del_idle[count] = idle[count] - idle_old[count];
      // calculate the percent usage for the current cpu
      percent_usage = ((del_total_tick[count] - del_idle[count]) / (double) del_total_tick[count]) * 100;
      if (count == 0)
      { printf ("Total CPU Usage: %3.2lf%%\n", percent_usage); }
      else
      { printf ("\tCPU%d Usage: %3.2lf%%\n", count - 1, percent_usage); 
		writeCSV(count - 1, percent_usage, "Usage");
		}
    }
    update_cycle++;
    printf ("\n");
  }
 
  /* Ctrl + C quit, therefore this will not be reached. We rely on the kernel to close this file */
  fclose (fp);
 
  return 0;
}
