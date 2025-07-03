#include <stdio.h>      
#include <stdlib.h>     
#include <unistd.h>     // POSIX: sleep, usleep
#include <pthread.h>    // Threads and mutex
#include <semaphore.h>  // Semaphore usage
#include <signal.h>     // Signal handling
#include <string.h>
#include <time.h>
#include <stdarg.h>     // Variadic functions (used in logging)

#define EAST 1          // Direction constant for EAST
#define WEST 2          // Direction constant for WEST
#define MAX_CARS 30     // Max num of cars in each queue
#define LANE_CAPACITY 2 // Number of cars allowed in shared construction
#define CROSSING_TIME 3 // Time for car to cross
#define LOG_LINES 8

// Sturcture of car
typedef struct {
    int id;             // Car ID
    int direction;      
    int active;         // Whether car is in consturction lane
    int progress;       // How far the car has crossed
    int is_ambulance;   // Flag ambulance
} Car;

sem_t construction_slots;   // Semaphore to limit cars entering consturction zone
pthread_mutex_t data_mtx = PTHREAD_MUTEX_INITIALIZER;   // Mutex to protect shared data
pthread_mutex_t log_mtx = PTHREAD_MUTEX_INITIALIZER;    // Mutex to protect logging
volatile sig_atomic_t stop = 0;     // Flag to indicte simulation stop

Car east_queue[MAX_CARS], west_queue[MAX_CARS]; // Queues for each direction
Car construction[LANE_CAPACITY];    // Consturction zone shared lane

int east_count = 0, west_count = 0; // Current car num
int east_next_id = 1, west_next_id = 1; // Assign next id
int green_dir = EAST;   // Initial green light direction
int tick = 0;
int lane_dir = -1;

char log_buf[LOG_LINES][128];   // store recent events
int log_idx = 0;

// Logging function (protected by Mutex)
void log_event(const char *fmt, ...) {
    pthread_mutex_lock(&log_mtx);   // Lock log buffer before writing (mutex)
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buf[log_idx], sizeof(log_buf[log_idx]), fmt, args);
    va_end(args);
    log_idx = (log_idx + 1) % LOG_LINES;
    pthread_mutex_unlock(&log_mtx); // Unlock log buffer (mutex)
}

// Screen clearing for UI Refresh
void clear_screen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void sigint_handler(int sig) {
    stop = 1;   // Stop when Ctrl+C
}

// Add a new car to queue (protected by Mutex)
void enqueue_car(int dir, int is_ambulance) {
    pthread_mutex_lock(&data_mtx);  // Lock shared queue data (mutex)
    Car new_car = {
        .id = (dir == EAST ? east_next_id++ : west_next_id++),
        .direction = dir,
        .active = 1,
        .progress = 0,
        .is_ambulance = is_ambulance  // Set ambulance flag
    };
    if (dir == EAST && east_count < MAX_CARS) {
        east_queue[east_count++] = new_car; // Add to EAST queue
    } else if (dir == WEST && west_count < MAX_CARS) {
        west_queue[west_count++] = new_car; // Add to WEST queue
    }
    pthread_mutex_unlock(&data_mtx);    // Unlock shared queue data (mutex)
    log_event("New %s %c%02d queued.", is_ambulance ? "ambulance" : "car", dir == EAST ? 'E' : 'W', new_car.id);
}

// Display Queue rows in UI
void print_row_with_padding(const char* label, const char prefix, Car* queue, int count) {
    char row_buf[128] = {0};
    int offset = snprintf(row_buf, sizeof(row_buf), " %s", label);
    for (int i = 0; i < 8; ++i) {
        if (i < count) {
            if (queue[i].is_ambulance)
                offset += snprintf(row_buf + offset, sizeof(row_buf) - offset, "\033[1;31m[AMB]\033[0m ");
            else
                offset += snprintf(row_buf + offset, sizeof(row_buf) - offset, "[%c%02d] ", prefix, queue[i].id);
        } else {
            offset += snprintf(row_buf + offset, sizeof(row_buf) - offset, "      ");
        }
    }
    int padding = 78 - offset;
    if (padding < 0) padding = 0;
    printf("|%s%*s|\n", row_buf, padding, "");
}

// Drawing UI (with Mutex Locks)
void draw_ui() {
    clear_screen();
    // Display student IDs (bold text, Ubuntu/Linux C-style look)
    printf("+------------------------------------------------------------------------------+\n");
    printf("|                              \033[1mStudent IDs:\033[0m                                    |\n");
    printf("|     [CST2209663] [CST2209659] [CST2209665] [CST2209183] [CST2209301]         |\n");
    printf("+------------------------------------------------------------------------------+\n\n");
    printf("+------------------------------------------------------------------------------+\n");
    printf("|                \033[1;33m2D TRAFFIC SIMULATION (Press Ctrl+C to exit)\033[0m                  |\n");
    printf("|------------------------------------------------------------------------------|\n");
    printf("| Green Light: %-4s | EAST Q: %02d | WEST Q: %02d | Tick: %2ds                      |\n",
        (green_dir == EAST ? "EAST" : "WEST"), east_count, west_count, tick);
    printf("|------------------------------------------------------------------------------|\n");

    pthread_mutex_lock(&data_mtx);  // Lock car queues (mutex)
    print_row_with_padding("WEST QUEUE:", 'W', west_queue, west_count);
    pthread_mutex_unlock(&data_mtx); // Unlock

    printf("|                              |== CONSTRUCTION ==|                            |\n");

    char lane_buf[80] = {0};
    int len = 0;
    int i, p;

    pthread_mutex_lock(&data_mtx);  // Lock lane data (mutex)
    for (i = 0; i < LANE_CAPACITY; ++i) {
    if (construction[i].active) {
        // Show AMBULANCE in red
        if (construction[i].is_ambulance) {
            len += snprintf(lane_buf + len, sizeof(lane_buf) - len, "\033[1;31m[AMB][");
        } else {
            len += snprintf(lane_buf + len, sizeof(lane_buf) - len, " %c%02d[",
                            construction[i].direction == EAST ? 'E' : 'W', construction[i].id);
        }

        // Show progress bar
        for (p = 0; p < CROSSING_TIME; ++p)
            lane_buf[len++] = (p < construction[i].progress) ? '=' : ' ';
        
        // Close bracket and reset color if needed
        if (construction[i].is_ambulance)
            len += snprintf(lane_buf + len, sizeof(lane_buf) - len, "]\033[0m ");
        else
            len += snprintf(lane_buf + len, sizeof(lane_buf) - len, "] ");
    } else {
        len += snprintf(lane_buf + len, sizeof(lane_buf) - len, "       ");
    }
    }
    pthread_mutex_unlock(&data_mtx); // Unlock
    
    int visual_len = 0;
    for (int i = 0; i < LANE_CAPACITY; ++i) {
        if (construction[i].active) {
            if (construction[i].is_ambulance) {
                visual_len += strlen("[AMB][") + CROSSING_TIME + strlen("] ");
            } else {
                visual_len += strlen(" E00[") + CROSSING_TIME + strlen("] ");
            }
        } else {
            visual_len += strlen("       ");
        }
    }
    int margin = (78 - 31 - visual_len);
    if (margin < 0) margin = 0;
    printf("|                              |%s%*s|\n", lane_buf, margin, "");

    pthread_mutex_lock(&data_mtx);  // Lock to show EAST queue
    print_row_with_padding("EAST QUEUE:", 'E', east_queue, east_count);
    pthread_mutex_unlock(&data_mtx);  // Unlock

    printf("+------------------------------------------------------------------------------+\n");
    printf("| Recent Events:                                                               |\n");
    pthread_mutex_lock(&log_mtx);   // Lock log buffer
    int idx = log_idx;
    for (i = 0; i < LOG_LINES; ++i) {
        int li = (idx + i) % LOG_LINES;
        printf("| %-76s |\n", log_buf[li]);
    }
    pthread_mutex_unlock(&log_mtx); // Unlock
    printf("+------------------------------------------------------------------------------+\n");
}

// Thread function (simulation timer)
void* tick_loop(void* arg) {
    while (!stop) {
        sleep(1); // wait 2 second
        tick++;  // Increment time
    }
    return NULL;
}

// Switch lights
void* controller(void* arg) {
    while (!stop) {
        sleep(CROSSING_TIME * 1.4); // Switch light every crossing time
        pthread_mutex_lock(&data_mtx);  // Lock to safely change green direction
        green_dir = (green_dir == EAST) ? WEST : EAST;
        log_event("Light switched to %s", (green_dir == EAST ? "EAST" : "WEST"));
        pthread_mutex_unlock(&data_mtx);
    }
    return NULL;
}

// Car lifecycle
void* car_thread(void* arg) {
    Car* car = (Car*)arg;
    while (!stop) {
        pthread_mutex_lock(&data_mtx);
        int allowed = (car->direction == green_dir && (lane_dir == -1 || lane_dir == car->direction));
        pthread_mutex_unlock(&data_mtx);
        if (allowed) break;
        usleep(100 * 1000);
    }

    sem_wait(&construction_slots); // Acquire construction slot (Semaphore)

    pthread_mutex_lock(&data_mtx);  // Lock to place car into construction
    int i;
    for (i = 0; i < LANE_CAPACITY; ++i) {
        if (!construction[i].active) {
            construction[i] = *car;
            construction[i].active = 1;
            construction[i].progress = 0;
            if (lane_dir == -1) lane_dir = car->direction;
            if (car->is_ambulance)
                log_event("AMBULANCE from %s ENTERED construction.", car->direction == EAST ? "EAST" : "WEST");
            else
                log_event("Car %c%02d ENTERED construction.", car->direction == EAST ? 'E' : 'W', car->id);
            break;
        }
    }
    pthread_mutex_unlock(&data_mtx);

    for (int t = 0; t < CROSSING_TIME && !stop; ++t) {
        pthread_mutex_lock(&data_mtx);  // Update progress every second
        for (int j = 0; j < LANE_CAPACITY; ++j) {
            if (construction[j].active && construction[j].id == car->id && construction[j].direction == car->direction)
                construction[j].progress = t + 1;
        }
        pthread_mutex_unlock(&data_mtx);
        sleep(1);
    }

    pthread_mutex_lock(&data_mtx);  // Remove car from construction
    for (i = 0; i < LANE_CAPACITY; ++i) {
        if (construction[i].active && construction[i].id == car->id && construction[i].direction == car->direction) {
            construction[i].active = 0;
            construction[i].progress = 0;
            if (car->is_ambulance)
                log_event("AMBULANCE from %s EXITED construction.", car->direction == EAST ? "EAST" : "WEST");
            else
                log_event("Car %c%02d EXITED construction.", car->direction == EAST ? 'E' : 'W', car->id);
            break;
        }
    }
    int still_active = 0;
    for (i = 0; i < LANE_CAPACITY; ++i)
        if (construction[i].active) still_active = 1;
    if (!still_active)
        lane_dir = -1;
    pthread_mutex_unlock(&data_mtx);

    sem_post(&construction_slots);  // Release construction slot (Semaphore)
    free(car);
    return NULL;
}

// Random car generator
void* generator(void* arg) {
    while (!stop) {
        int dir = rand() % 2 ? EAST : WEST;
        int is_ambulance = (rand() % 10 == 0);
        enqueue_car(dir, is_ambulance);   // create new car or ambulance
        sleep(1 + rand() % 2); // Set delays between cars
    }
    return NULL;
}

// Dispatcher sends car into consturction
void* dispatcher(void* arg) {
    while (!stop) {
        pthread_mutex_lock(&data_mtx);  // Lock shared queues
        Car* queue = (green_dir == EAST) ? east_queue : west_queue;
        int* count = (green_dir == EAST) ? &east_count : &west_count;
        if (*count > 0 && (lane_dir == -1 || lane_dir == green_dir)) {
            for (int i = 0; i < LANE_CAPACITY; ++i) {
                if (!construction[i].active) {
                    // Check if ambulance exists in queue
                    int ambulance_index = -1;
                    for (int k = 0; k < *count; ++k) {
                        if (queue[k].is_ambulance) {
                            ambulance_index = k;
                            break;
                        }
                    }
                    int index = (ambulance_index != -1) ? ambulance_index : 0; // Prioritize ambulance if found

                    Car car = queue[index];  // Pick the ambulance or first normal car

                    // Shift queue to remove selected car
                    for (int j = index + 1; j < *count; ++j)
                        queue[j - 1] = queue[j];
                    (*count)--;

                    Car* new_car = malloc(sizeof(Car));
                    *new_car = car;
                    pthread_t tid;
                    pthread_create(&tid, NULL, car_thread, new_car); //Spawn car thread
                    pthread_detach(tid);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&data_mtx);  // Unlock
        usleep(300 * 1000); // Wait before checking again
    }
    return NULL;
}

// User interface update thread
void* ui_loop(void* arg) {
    while (!stop) {
        draw_ui();
        usleep(200 * 1000);
    }
    return NULL;
}

// Program setup
int main() {
    signal(SIGINT, sigint_handler); // Register SIGNIT handler (Ctrl+c)
    srand(time(NULL));  // Random for car direction

    sem_init(&construction_slots, 0, LANE_CAPACITY);  // Initiate semmaphore to control lane access
    memset(log_buf, 0, sizeof(log_buf));
    log_event("Simulation started.");

    // Initialize EAST and WEST queues with 3 cars each
    pthread_mutex_lock(&data_mtx);  // Lock shared data (mutex)

    for (int i = 0; i < 3; ++i) {
        Car car = {
            .id = east_next_id++,
            .direction = EAST,
            .active = 1,
            .progress = 0,
            .is_ambulance = 0 
        };
        east_queue[east_count++] = car;
        log_event("Preloaded %s E%02d queued.", car.is_ambulance ? "ambulance" : "car", car.id);
    }

    for (int i = 0; i < 3; ++i) {
        Car car = {
            .id = west_next_id++,
            .direction = WEST,
            .active = 1,
            .progress = 0,
            .is_ambulance = 0
        };
        west_queue[west_count++] = car;
        log_event("Preloaded %s W%02d queued.", car.is_ambulance ? "ambulance" : "car", car.id);
    }

    pthread_mutex_unlock(&data_mtx);  // Unlock shared data (mutex)
    // Declare threads
    pthread_t t_ctrl, t_ui, t_gen, t_disp, t_tick;

    // process 1: Light-switch controller thread
    pthread_create(&t_ctrl, NULL, controller, NULL);

    // process 2: UI refresh thread
    pthread_create(&t_ui, NULL, ui_loop, NULL);

    // process 3: Random vehicles generator thread
    pthread_create(&t_gen, NULL, generator, NULL);

    // process 4: Dispatcher thread (send cars into construction)
    pthread_create(&t_disp, NULL, dispatcher, NULL);

    // process 5: Simulation timer thread (increments tick)
    pthread_create(&t_tick, NULL, tick_loop, NULL);

    pthread_join(t_ctrl, NULL);
    pthread_join(t_ui, NULL);
    pthread_join(t_gen, NULL);
    pthread_join(t_disp, NULL);
    pthread_join(t_tick, NULL);

    sem_destroy(&construction_slots);   // Destroy semaphore
    printf("Simulation ended.\n");
    return 0;
}
