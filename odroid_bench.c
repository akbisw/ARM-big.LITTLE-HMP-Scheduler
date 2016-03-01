/* Show per core CPU utilization of the system
 * This is a part of the post http://phoxis.org/2013/09/05/finding-overall-and-per-core-cpu-utilization
 */
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#define BUF_MAX 1024
#define MAX_CPU 128

/* Read CPU Fields from /proc/stat */
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

/* Write the CPU Usage metrics to individual files */
/* (CPU num, the metric value, the metric type) */
void writeCSV(int cpu_num, float value, char* metric_name, char* metric_type)
{
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char file_name[14];
        sprintf(file_name, "%s%d_%s.csv", metric_name, cpu_num, metric_type);
        FILE *f_ptr = fopen(file_name, "a");
        fprintf(f_ptr, "%d-%d-%d %d:%d:%d,%3.2lf\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, value);
        fclose(f_ptr);
}

/* Read CPU Temperatures */
int cpu_temp (FILE *fp, int *temp)
{
  int retval;
  char buffer[20];

  if (!fgets (buffer, 20, fp))
  { perror ("Error"); }
  /* line is sensor : value. */
  // * skips the match.
  retval = sscanf (buffer, "s%*s : %d", temp);
  if (retval == 0)
  { return -1; }
  if (retval == 1) /* Atleast 4 fields is to be read */
  { return 1; }
}

/* Read CPU POWER! */
int cpu_power (FILE *fp, float *power)
{
  int retval;
  char buffer[20];

  if (!fgets (buffer, 20, fp))
  { perror ("Error"); }
  /* line is sensor : value. */
  // * skips the match.
  retval = sscanf (buffer, "%f", power);
  if (retval == 0)
  { return -1; }
  if (retval == 1) /* Atleast 4 fields is to be read */
  { return 1; }
}

int main (void)
{
  FILE *cpu_usage_fptr;
  unsigned long long int fields[10], total_tick[MAX_CPU], total_tick_old[MAX_CPU], idle[MAX_CPU], idle_old[MAX_CPU], del_total_tick[MAX_CPU], del_idle[MAX_CPU];
  int update_cycle = 0, i, cpus = 0, count;
  double percent_usage;

  /* FILE POINTER FOR BIG CPU TEMP */
  FILE *cpu_temp_fptr;
  int temp, big_cpu_count;
  cpu_temp_fptr = fopen ("/sys/devices/10060000.tmu/temp", "r");

  /* CPU Timings */
  cpu_usage_fptr = fopen ("/proc/stat", "r");
  if (cpu_usage_fptr == NULL)
  {
    perror ("Error");
  }

  /* Voltage/AMP/Watt Readings */
  // A7VAW A15VAW
  float power_readings[6];
  FILE *a7_volt, *a7_amp, *a7_watt, *a15_volt, *a15_amp, *a15_watt;
  a7_volt = fopen ("/sys/bus/i2c/drivers/INA231/3-0045/sensor_V", "r");
  a7_amp = fopen ("/sys/bus/i2c/drivers/INA231/3-0045/sensor_A", "r");
  a7_watt = fopen ("/sys/bus/i2c/drivers/INA231/3-0045/sensor_W", "r");
  a15_volt = fopen ("/sys/bus/i2c/drivers/INA231/3-0040/sensor_V", "r");
  a15_amp = fopen ("/sys/bus/i2c/drivers/INA231/3-0040/sensor_A", "r");
  a15_watt = fopen ("/sys/bus/i2c/drivers/INA231/3-0040/sensor_W", "r");
  
  int power_count;
  float power_val;
  FILE *powerfptr[6] = {a7_volt, a7_amp, a7_watt, a15_volt, a15_amp, a15_watt}; 
  /* Parse the /proc/stat file until you find cpu */
  // This loops until it reads all 9 cpu lines
  // Goal is to count how many cpus the system has
  while (read_fields (cpu_usage_fptr, fields) != -1)
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
    /* Seek /proc/stat to the beginning of the /proc/stat file for a BRAND NEW CPU READING */
    fseek (cpu_usage_fptr, 0, SEEK_SET);
    /* Flush the file pointer to update the new cpu values stored by kernel */
    fflush (cpu_usage_fptr);

    /*CPU TEMP FILE POINTER SEEK TO BEGINNING*/
    fseek (cpu_temp_fptr, 0, SEEK_SET);
    /* Flush the file pointer to update the new cpu values stored by kernel */
    fflush (cpu_temp_fptr);
    /* CPU Power File Pointers seek to beginning  */
    for (power_count = 0; power_count < 6; power_count++)
    {
      fseek (powerfptr[power_count], 0, SEEK_SET);    
      fflush (powerfptr[power_count]);
    }

    // keep count of how many time cpu stats has been updated
    printf ("[Update cycle %d]\n", update_cycle);
    // For loop to collect and measure CPU USAGE.
    for (count = 0; count < cpus; count++)
    {
      // Store the old field counters
      total_tick_old[count] = total_tick[count];
      idle_old[count] = idle[count];
      // read new counter values
      if (!read_fields (cpu_usage_fptr, fields))
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
      { 
        printf ("Total CPU Usage: %3.2lf%%\n", percent_usage); 
      }
      else
      { 
        printf ("\tCPU%d Usage: %3.2lf%%\n", count - 1, percent_usage);
        writeCSV(count - 1, percent_usage, "CPU", "Usage");
      }
    }
    /*  Collect and store the CPU Temperature Data  */
    for (big_cpu_count = 4; big_cpu_count < 8; big_cpu_count++)
    {
      cpu_temp(cpu_temp_fptr, &temp);
      printf("\tCPU%d %s: %d\n", big_cpu_count, "Temp", temp/1000);
      writeCSV(big_cpu_count, temp, "CPU", "Temp");
    }
    // GPU TEMP
    cpu_temp(cpu_temp_fptr, &temp);
     
    /* Power Readings of A7 cluster and A15 cluster */
    cpu_power(a7_volt, &power_val);
    printf("\ta7 %s: %0.4f\n", "Voltage", power_val);
    writeCSV(7, power_val, "a", "Voltage");

    cpu_power(a7_amp, &power_val);
    printf("\ta7 %s: %0.4f\n", "Amp", power_val);
    writeCSV(7, power_val, "a", "Amp");

    cpu_power(a7_watt, &power_val);
    printf("\ta7 %s: %0.4f\n", "Watt", power_val);
    writeCSV(7, power_val, "a", "Watt");

    cpu_power(a15_volt, &power_val);
    printf("\ta15 %s: %0.4f\n", "Voltage", power_val);
    writeCSV(15, power_val, "a", "Voltage");

    cpu_power(a15_amp, &power_val);
    printf("\ta15 %s: %0.4f\n", "Amp", power_val);
    writeCSV(15, power_val, "a", "Voltage");

    cpu_power(a15_watt, &power_val);
    printf("\ta15 %s: %0.4f\n", "Watt", power_val);
    writeCSV(15, power_val, "a", "Watt");
    
    update_cycle++;
    printf ("\n");
  }

  /* Ctrl + C quit, therefore this will not be reached. We rely on the kernel to close this file */
  fclose (cpu_usage_fptr);
  fclose (cpu_temp_fptr);
  for (power_count = 0; power_count < 6; power_count++)
  {
    fclose (powerfptr[power_count]);
  }
  return 0;
}
