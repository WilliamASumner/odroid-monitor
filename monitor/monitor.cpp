#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <string>
#include <fstream>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ptrace.h>
#include <sys/ioctl.h>

#include <sched.h>
#include <fcntl.h>

#include "monitor.h"
#include "circ_buff.h"

int signal_cleanup = 0; // edited by signals to force main loop to cleanup

void print_args(char **arg_list, int num_args) {
    for (int i = 0; i < num_args; i++) {
        if (arg_list[i] == NULL)
            printf("NULL ");
        else
            printf("%s ",arg_list[i]);
    }
    printf("\n");
}

void free_args(char **arg_list, int num_args) {
    for(int i = 0; i < num_args; i++) {
        free(arg_list[i]);
    }
    free(arg_list);
}

void toggle_sensors(struct odroid_state * state, char state_code) {
    int i,result;
    char code = state_code;
    for (i = 0; i < NUM_SENSORS; i++) {
        result = write(state->enable_fds[i],&code,sizeof(code));
        if (result != sizeof(code)) {
            fprintf(stderr,"error toggling sensors!\n");
            fprintf(stderr,"Error %d: %s\n",errno,strerror(errno));
            return;
        }
    }
}

void init_odroid_state(struct odroid_state * state) {
    state->enable_fds[0] = open(SENSOR_ENABLE(0045),O_RDWR); // setup enables
    state->enable_fds[1] = open(SENSOR_ENABLE(0040),O_RDWR);
    state->enable_fds[2] = open(SENSOR_ENABLE(0041),O_RDWR);
    state->enable_fds[3] = open(SENSOR_ENABLE(0044),O_RDWR);

    int i;
    for (i = 0; i < NUM_SENSORS; i++) {
        if (state->enable_fds[i] == -1 || !state->enable_fds[i]) {
            fprintf(stderr,"Error: could not init odroid_state! Unable to open enables\n");
            return;
        }
    }

    toggle_sensors(state,SENSOR_START); // turn the sensors on
    printf("waiting for sensors to warm up...\n");
    sleep(2); // allow sensors to warm up


    state->read_fds[0] = open(SENSOR_W(0045),O_RDONLY); // setup the reading of the sensors
    state->read_fds[1] = open(SENSOR_W(0040),O_RDONLY);
    state->read_fds[2] = open(SENSOR_W(0041),O_RDONLY);
    state->read_fds[3] = open(SENSOR_W(0044),O_RDONLY);

    for (i = 0; i < NUM_SENSORS; i++) {
        if (state->read_fds[i] == -1 || !state->read_fds[i]) {
            fprintf(stderr,"Error: could not init odroid_state! Unable to open sensor data\n");
            return;
        }
    }
    printf("finished initializing sensors!\n");
    

}

void end_odroid_state(struct odroid_state * state) { // cleanup
    int i;
    toggle_sensors(state,SENSOR_END);
    for (i = 0; i < NUM_SENSORS; i++) { // close file descriptors
        close(state->read_fds[i]);
        close(state->enable_fds[i]);
    }
}

int get_power(struct odroid_state * state, cbuf_handle_t handle) {
    int i;
    char raw_data[10];
    for (i = 0; i < NUM_SENSORS; i++) {
        if (pread(state->read_fds[i], raw_data,sizeof(raw_data),0) > 0) {
            raw_data[8] = ';'; // null terminate
            raw_data[9] = 0; // null terminate
            if (circular_buf_put_bytes(handle,(u_int8_t *)raw_data, strlen(raw_data)) != strlen(raw_data)) {
                fprintf(stderr,"error: unable to write to buffer\n");
                return -1;
            }
        }
    }
    return 0;
}

int get_cid(int pid,int tid) { // opens stats file and gets cid of task
    char filename[100];
    int cid = -1;
    snprintf(filename,100,"/proc/%d/task/%d/stat",pid,tid);
    std::ifstream stat_file(filename);
    if (!stat_file) {
#ifdef DEBUG
        fprintf(stderr,"error opening stat file: %s\n",filename);
        fprintf(stderr,"Error %d: %s\n",errno,strerror(errno));
#endif
        stat_file.close();
        return -1;
    }
    std::string line;
    if (getline(stat_file,line)) {
        std::size_t index,prev;
        int field_number = 2;   // the name field is the second one
        index = line.find(')'); // find the end of the name field
        prev = index;
        index = line.find(' ',index);
        while(index != std::string::npos && field_number < 39) {
            prev = index+1;
            index = line.find(' ',index+1);
            field_number += 1;
        }
        if (prev != std::string::npos && index != std::string::npos)
            cid = std::stoi(line.substr(prev,index-prev));
        else {
#ifdef DEBUG
            fprintf(stderr,"error: the index of the CID was not found\n");
#endif
            stat_file.close();
            return -1;
        }
    } else { // likely task ended before we could read it
#ifdef DEBUG
        fprintf(stderr,"error: could not read stat file: %s\n",filename);
#endif
        stat_file.close();
        return -1;
    }
    stat_file.close();
    return cid;
}

int get_timestamp(cbuf_handle_t handle) { // just writes the timestamp
    struct timeval retrieve; // timestamp
    char time_str[100];
    gettimeofday(&retrieve,0); // get timestamp
    snprintf(time_str,100,"%ld%06ld:",retrieve.tv_sec, retrieve.tv_usec); // write down the time of measurement in the standard format
    if (circular_buf_put_bytes(handle,(u_int8_t *)time_str, strlen(time_str)) != strlen(time_str)) {
        fprintf(stderr,"error: unable to write to buffer\n");
        return -1;
    }
    return 0;
}

int get_cpu_config(int proc_pid, cbuf_handle_t handle) { // writes the cpu_config
    DIR *proc_dir;
    char dirname[100];
    char cid_str[1024];
    int tid,cid;

    snprintf(dirname, sizeof(dirname), "/proc/%d/task/",proc_pid);
    proc_dir = opendir(dirname);

    if (proc_dir) {
        struct dirent *entry;
        while ((entry = readdir(proc_dir)) != NULL) { /* for each thread id */
            if(entry->d_name[0] == '.') // if the default entries
                continue;

            tid = atoi(entry->d_name);
            cid = get_cid(proc_pid,tid);
            if (cid == -1) {
#ifdef DEBUG
                fprintf(stderr,"error: unable to get core id\n");
#endif
                continue;
            }
            snprintf(cid_str,sizeof(cid_str),"(%d,%d);",tid,cid); // write tid,cid pairs
            if (circular_buf_put_bytes(handle,(u_int8_t *)cid_str, strlen(cid_str)) != strlen(cid_str)) {
                fprintf(stderr,"error: unable to write to buffer\n");
                closedir(proc_dir);
                return -1;
            }
        }
    } else {
#ifdef DEBUG
        fprintf(stderr,"error: unable to read process directory %s\n",dirname);
#endif
        closedir(proc_dir);
        return -1;
    }
    if (circular_buf_put_byte_checked(handle,'\n') != 0) { // write \n between lists
        fprintf(stderr,"error: unable to write to buffer\n");
        closedir(proc_dir);
        return -1;
    }
    closedir(proc_dir);
    return 0;
}

void sig_handler(int signo) { 
    switch (signo) {
        case SIGINT: // CTRL C
            fprintf(stderr,"caught SIGINT\n");
            signal_cleanup = 1;
            break;
        case SIGTERM: // another process is killing us
            fprintf(stderr,"caught SIGTERM\n");
            signal_cleanup = 1;
            break;
        default:
            fprintf(stderr,"error: improper signal found, unable to handle\n");
            exit(-1);
    }
}

int collect_stats(cbuf_handle_t file_handle, char mode, int pid, struct odroid_state * state) {
	if (get_timestamp(file_handle) != 0)
		return -1;
    switch (mode) {
        case 'e': // energy
			if (get_power(state,file_handle) != 0)
				return -1;
			break;
        case 'c': // cpu config
            if (get_cpu_config(pid,file_handle) != 0 )
                return -1;
            break;
        case 'b': // both
            if (get_power(state,file_handle) != 0)
                return -1;
            if (get_cpu_config(pid,file_handle) != 0)
                return -1;
        default:
            break;
    }
    return 0;
}

void close_circ_buffer(cbuf_handle_t handle) {
    if (handle) {
        circular_buf_write(handle); // write out the data
        if (handle->file) {
            fclose(handle->file); // close the file
            handle->file = NULL;
        }
        if (handle->buffer) {
            free(handle->buffer); // free the buffer
            handle->buffer = NULL;
        }
        circular_buf_free(handle); // free the handle
    }
}

void print_usage(const char * name) {
        fprintf(stderr,"usage: %s [interval] [pid to attach or -1] [mode] [log-name] [command...]\n",name);
}

int main(int argc, char **argv) {

    // SIGNAL CATCHING
    struct sigaction new_action;
    new_action.sa_handler = sig_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;

    if (sigaction(SIGINT,&new_action,NULL) == -1) {
        fprintf(stderr,"error: cannot catch SIGINT\n");
        return -1;
    }
    if (sigaction(SIGTERM,&new_action,NULL) == -1) {
        fprintf(stderr,"error: cannot catch SIGTERM\n");
        return -1;
    }

    // Read in values from command line
    // ./monitor [interval] [pid or -1 for attach] [mode] [log-name] command
    //
    if (argc < 5) {
        fprintf(stderr,"error: not enough arguments\n");
        print_usage(argv[0]);
        return -1;
    }

    struct timespec del,rem; // first arg
    del.tv_sec = 0;
    del.tv_nsec = 1000L * atol(argv[1]); // convert us seconds to proper ns

    int pid = atoi(argv[2]); // second arg
    int will_attach = 0;

    int mode = argv[3][0]; // third arg

    char out_filename[100];
    if (strnlen(argv[4],101) > 100) {
        fprintf(stderr,"error: outfile name is too long (> 100 chars)\n");
        return -1;
    }
    strncpy(out_filename,argv[4],100); // fourth arg

    FILE * file = fopen (out_filename, "w");
    if (!file) {
        fprintf(stderr,"error: could not open '%s', Error %d:%s\n",out_filename,errno,strerror(errno));
        return -1;
    }

    // Initialize the monitor state
    int buffer_size = 104857600; // 100 mb buffer limit
    u_int8_t * buffer = (u_int8_t *)malloc(sizeof(u_int8_t) * buffer_size);
    char prog_name[100];
    char header_str[100];
    cbuf_handle_t file_handle = circular_buf_init(buffer,buffer_size,file);

    switch(mode) {
        case 'e': 
            strncpy(header_str,"Timestamp:SENS_A7;SENS_A15;SENS_MEM;SENS_GPU;\n",100);
            break;
        case 'c':
            strncpy(header_str,"Timestamp:(TID,CORE_ID);[(TID,CORE_ID);...]\n",100);
            break;
        case 'b':
            strncpy(header_str,"Timestamp:SENS_A7;SENS_A15;SENS_MEM;SENS_GPU;(TID,CORE_ID);[(TID,CORE_ID);...]\n",100);
            break;
        default:
            fprintf(stderr,"error: invalid mode supplied, must be e, c, or b\n");
            return -1;
    }

    // Try writing to the circular buffer
    if (circular_buf_put_bytes(file_handle,(u_int8_t *)header_str, strlen(header_str)) != strlen(header_str)) {
        fprintf(stderr,"error writing to results buffer\n");
        return -1;
    }

    // Processing
    char **new_argv;
    int num_args_to_skip = 5;
    if (argc - num_args_to_skip < 0 ) {
        print_usage(argv[0]);
        close_circ_buffer(file_handle);
        return -1;
    }

    if (pid == -1) { // user wants to attach to new pid from command rather than existing pid
        new_argv = (char **)malloc((argc-num_args_to_skip)*sizeof(char *)); // new arglist
        if (new_argv == NULL) {
            fprintf(stderr,"Error: error allocating memory\n");
            close_circ_buffer(file_handle);
            return -1;
        } else if (argc < 3 || strnlen(argv[num_args_to_skip],101) > 100) {
            fprintf(stderr,"Error: improperly formatted command\n");
            print_usage(argv[0]);
            close_circ_buffer(file_handle);
            return -1;
        }
        strncpy(prog_name,argv[num_args_to_skip],100);

        for (int i = num_args_to_skip; i < argc; i++) {
            new_argv[i-num_args_to_skip] = (char *)malloc(100); // 100 character limit on individual args
            if (new_argv[i-num_args_to_skip] == NULL)  {
                fprintf(stderr, "Error copying new argv array\n");
                free_args(new_argv,i-num_args_to_skip); // just cleanup what we can
                close_circ_buffer(file_handle);
                return -1;
            }
            strncpy(new_argv[i-num_args_to_skip],argv[i],100);
        }
        new_argv[argc-num_args_to_skip] = (char *)NULL; // arglist must be terminated by NULL
        pid = fork();
    } else if (atoi(argv[2]) > 0) {
        will_attach = 1;
    } else {
        fprintf(stderr,"error: invalid ID supplied\n");
        close_circ_buffer(file_handle);
        return -1;
    }

    if (pid == 0) { // child
        print_args(new_argv,argc-num_args_to_skip);
        execvp(prog_name,new_argv);
        fprintf(stderr,"unable to execute program %s\n",prog_name);
        return -1;
    } else { // parent of the fork
#ifdef __linux__
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(0,&mask); // set to same big core every time to increase predictablity
        sched_setaffinity(0,sizeof(mask),&mask);

        if (will_attach) {
            if (ptrace(PTRACE_SEIZE,pid, NULL, NULL) == -1){ // attach to that pid, but dont stop it
                fprintf(stderr,"error: could not seize process: %d\n",pid);
                fprintf(stderr,"try running as root, alternatively process may not exist\n");
                close_circ_buffer(file_handle);
                return -1;

            }
            printf("succesfully attached\n");
        }
#else
        if (will_attach) {
            fprintf(stderr,"error: attach functionality not yet implemented for this system\n");
            close_circ_buffer(file_handle);
            return -1;
        }
#endif

        int status;
        pid_t return_pid = waitpid(pid, &status, WNOHANG);
        int error = 0;
        struct odroid_state * state = (struct odroid_state *)malloc(sizeof(struct odroid_state));
        if (mode != 'c') // if not cpu_config_only
            init_odroid_state(state);

        while ((return_pid = waitpid(pid,&status,WNOHANG)) == 0 && !error && !signal_cleanup) { // while the thread isnt done
            error = collect_stats(file_handle,mode,pid,state);
            nanosleep(&del,&rem); // sleep for the requisite amount of time
        }
        if (return_pid < 0 && !will_attach )
            fprintf(stderr,"Error running child process\n");

        // cleanup!
        close_circ_buffer(file_handle);
        //if (!will_attach)
        //  free_args(new_argv,argc - num_args_to_skip); // not sure if these needs to be done?
        if (mode != 'c')
            end_odroid_state(state);
        free(state);
    }
    return 0;
}
