#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>

#define INPUT_SIZE 100

#define COMPULSORY_FAULT 1111
#define CAPACITY_FAULT 2222
#define NO_FAULT 3333

struct table_str {
    int page_number;
    int reference_bit;
    int validity_bit;
};

int main(int argc, char *argv[]) {
    char replacement_policy[50], page_table_size[10];
    struct timeval tv;
    char *policy_str[4] = {"First In First Out", "Least Recently Used", "Second Chance", "Invalid Entry"};
    char *policy_str_short[4] = {"FIFO", " LRU", " SEC", " INV"};
    enum {
        FIFO, LRU, SEC, INVALID
    } policy = INVALID;
    int table_size = 0;

    printf("\n \n");
    printf(" -------- PROGRAM STARTING ----------------- \n");
    printf(" -------- ------------------ --------------- \n\n");

    /***************  Initialization sequence  ************************************/
    printf("Please enter a page replacement policy: ");
    do {
        fgets(replacement_policy, 50, stdin);
        replacement_policy[strcspn(replacement_policy, "\n")] = 0;
        if (replacement_policy[0] > '0' && replacement_policy[0] < '4') {
            policy = replacement_policy[0] - '1';
        } else if (strcasecmp(replacement_policy, "LRU") == 0 ||
                   strcasecmp(replacement_policy, "Least recently used") == 0) {
            policy = LRU;
        } else if (strcasecmp(replacement_policy, "FIFO") == 0 ||
                   strcasecmp(replacement_policy, "First In First Out") == 0) {
            policy = FIFO;
        } else if (strcasecmp(replacement_policy, "SEC") == 0 || strcasecmp(replacement_policy, "Second Chance") == 0) {
            policy = SEC;
        } else {
            printf("\n\'%s\' is an invalid entry. Please enter something between: "
                   "\n\t 1. LRU or Least Recently Used"
                   "\n\t 2. FIFO or First In First Out,"
                   "\n\t 3. SEC or Second Chance \n", replacement_policy);
            printf("Please enter something else: ");
        }
    } while (policy == INVALID);
    printf("\n%s replacement policy selected \n", policy_str[policy]);
    printf("\n Select the output table size (4 or 8): ");

    // Get the page table size
    do {
        fgets(page_table_size, 10, stdin);
        page_table_size[strcspn(page_table_size, "\n")] = 0;
        if (strlen(page_table_size) == 1 && page_table_size[0] == '4' || page_table_size[0] == '8') {
            table_size = page_table_size[0] - '0';
        } else {
            printf("\n\'%s\' is an invalid entry. Please enter 4 or 8 \n", page_table_size);
            printf("Please enter the table size: ");
        }
    } while (table_size == 0);
    /**/

    struct stat sst = {0};
    if (stat("./temp", &sst) == -1) {
        mkdir("./temp", 0777);
    }

    printf("\n Starting Page replacement algorithm \n");

    int out_file_ind = table_size / 8;
    // ******************  Table structure initialization  ****************

    struct table_str *page_table = (struct table_str *) malloc(table_size * sizeof(struct table_str));
    for (int i = 0; i < table_size; i++) {
        page_table[i].reference_bit = 0;
        page_table[i].validity_bit = 0;
        page_table[i].page_number = -1;
    }

    /********************** File Input Output ****************************/
    const char *inp_file_name = "dokanias_proj2_input.txt";

    FILE *input_file = fopen(inp_file_name, "r");
    int input_page_nums[INPUT_SIZE];
    int pn_index = 0;
    // Parse the input file
    if (input_file != NULL) {
        int p_number = -1;
        while (1) {
            char ch = fgetc(input_file);
            if (ch == EOF || pn_index == INPUT_SIZE)
                break;
            if (ch < '0' || ch > '9') {
                if (p_number != -1) {
                    input_page_nums[pn_index] = p_number;
                    ++pn_index;
                }
                p_number = -1;
            } else {
                if (p_number == -1)
                    p_number = 0;
                p_number = (p_number * 10) + (ch - '0');
            }
        }
        if (p_number != -1) {
            input_page_nums[pn_index] = p_number;
            ++pn_index;
        }
        fclose(input_file);
    }

    // If input file not proper, generate new input file
    if (pn_index != INPUT_SIZE) {
        printf("Regenerating the input file ... \n");
        /********* Writing 100 random integers to the input file ************/
        input_file = fopen(inp_file_name, "w+");
        gettimeofday(&tv, NULL);
        int t = (int) tv.tv_usec;
        srand(t);

        for (int i = 0; i < INPUT_SIZE; i++) {
            int rand_value = rand();
            if ((i > table_size) && (rand() % 11) == 0) {
                int prev = rand() % (table_size - 1) + 2;
                rand_value = input_page_nums[i - prev];
            }
            input_page_nums[i] = rand_value % 16;
            fprintf(input_file, "%d, ", input_page_nums[i]);
        }
        fclose(input_file);
    }

    // Handling the output of computations...
    int page_number;
    int page_index, fifo_repl_index=0, sec_repl_index=0, fault_type, temp_var;
    int total_faults = 0;

    char space[] = "                ";
    if (out_file_ind == 0) {
        space[0] = 0;
    }

    FILE *temp_file;
    const char *out_file_name[2] = {"dokanias_proj2_output_4 frames.txt",
                                    "dokanias_proj2_output_8 frames.txt"};
    char temp_name[100];
    sprintf(temp_name, "temp/temp_file_%s_%d", policy_str_short[policy], table_size);
    temp_file = fopen(temp_name, "w+");

    fprintf(temp_file,
            "Input   System Time (sec)    %s Table data     %s Page Fault (Type)\n",
            policy_str_short[policy], space);

    char replaced_str[100];
    for (int k = 0; k < INPUT_SIZE; ++k) {
        page_number = input_page_nums[k];
        // LOOKING FOR THE PAGE IN TABLE
        fprintf(temp_file, " %02d     ", page_number);
        page_index = -1;
        fault_type = CAPACITY_FAULT;
        for (int i = 0; i < table_size; i++) {
            // If any uninitialized entries
            if (page_table[i].validity_bit == 0) {
                page_table[i].page_number = page_number;
                page_table[i].validity_bit = 1;
                page_table[i].reference_bit = 1;
                page_index = i;
                fault_type = COMPULSORY_FAULT;
                break;
            }
            // Find any matching page entries
            if (page_table[i].page_number == page_number) {
                page_table[i].reference_bit = 1;
                page_index = i;
                fault_type = NO_FAULT;
                break;
            }
        }

        if (fault_type != NO_FAULT)
            total_faults++;

        // EXECUTING REPLACEMENT ALGORITHMS
        if (policy == LRU) {
            if (page_index != 0) {
                if (fault_type == CAPACITY_FAULT) {
                    temp_var = table_size - 1;
                    sprintf(replaced_str, "Replaced Page %02d with %02d",
                            page_table[temp_var].page_number,  page_number);
                } else {
                    temp_var = page_index;
                }
                // There was a fault_type, replace the last used page
                for (int y = temp_var; y > 0; y--) {
                    page_table[y] = page_table[y - 1];
                }
                page_table[0].page_number = page_number;
                page_table[0].reference_bit = 1;
            }
        } else if (policy == FIFO) {
            if (fault_type == CAPACITY_FAULT) {
                sprintf(replaced_str, "Replaced Page %02d with %02d",
                        page_table[fifo_repl_index].page_number,  page_number);
                page_table[fifo_repl_index].page_number = page_number;
                page_table[fifo_repl_index].reference_bit = 1;
                fifo_repl_index = (fifo_repl_index + 1) % table_size;
            }
        } else if (policy == SEC) {
            if (fault_type == CAPACITY_FAULT) {
                int z;
                for (int y = 0; y < (table_size + 2); y++) {
                    z = (y + sec_repl_index) % table_size;
                    if (page_table[z].reference_bit == 0) {
                        sprintf(replaced_str, "Replaced Page %02d with %02d",
                                page_table[z].page_number,  page_number);
                        page_table[z].page_number = page_number;
                        page_table[z].reference_bit = 1;
                        break;
                    } else {
                        page_table[z].reference_bit = 0;
                    }
                }
                sec_repl_index = (sec_repl_index + 1) % table_size;
            }
        }

        // Writing the output
        gettimeofday(&tv, NULL);
        fprintf(temp_file, "%ld.%03ld  \t|", tv.tv_sec, (tv.tv_usec / 1000));
        for (int y = 0; y < table_size; y++) {
            if (page_table[y].validity_bit != 0) {
                fprintf(temp_file, "  %02d", page_table[y].page_number);
            } else {
                fprintf(temp_file, "    ");
            }
        }
        fprintf(temp_file, " |   ");

        if (fault_type == NO_FAULT) {
            fprintf(temp_file, "               \n");
        } else if (fault_type == COMPULSORY_FAULT) {
            fprintf(temp_file, "PAGE FAULT (FIRST ENTRY -- COMPULSORY), total faults = %d\n", total_faults);
        } else {
            fprintf(temp_file, "PAGE FAULT (%s),  total faults = %d\n",
                    replaced_str, total_faults);
        }
    }
    fprintf(temp_file, "\nTotal faults for %s replacement policy = %d out of %d \n\n\n\n",
            policy_str[policy], total_faults, INPUT_SIZE);

    char *line_r = NULL;
    size_t temp_x = 0;

    fseek(temp_file, 0, SEEK_SET);
    while (getline(&line_r, &temp_x, temp_file) != -1) {
        printf("%s", line_r);
    }
    fclose(temp_file);

    FILE *output_file;
    FILE *t;
    for(int j=0; j<2; j++) {
        output_file = fopen(out_file_name[j], "w+");
        for(int i=0; i<3; i++) {
            memset(temp_name, 0, sizeof temp_name);
            sprintf(temp_name, "temp/temp_file_%s_%d", policy_str_short[i], 4*(j+1));
            t = fopen(temp_name, "r");
            if(t != NULL) {
                fprintf(output_file, "Running for %s replacement policy \n", policy_str[i]);
                while (getline(&line_r, &temp_x, t) != -1) {
                    fprintf(output_file, "%s", line_r);
                }
                if(i < 2)
                    fprintf(output_file, "%s", "_____________________________________________________ \n");
                fclose(t);
            }
        }
        fclose(output_file);
    }


    printf("\n \n");
    printf(" -------- END OF PROGRAM  ------------------ \n");
    printf(" -------- By: Sourav Dokania --------------- \n");
    printf(" ******************************************* \n");

    free(page_table);
    return 0;
}
