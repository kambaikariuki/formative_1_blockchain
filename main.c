#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

#define MAX_STUDENTS 100
#define SIGNATURE_SIZE 256
#define MAX_PENDING 1000

int pending_count = 0;

typedef struct {
    char student_id[20];
    char full_name[50];
    char course_code[10];
} Student;

typedef struct Block {
    int index;
    time_t timestamp;

    char student_id[20];
    char full_name[50];
    char course_code[10];
    char status[10];

    char previous_hash[65];

    unsigned char signature[SIGNATURE_SIZE];
    unsigned int signature_length;

    char hash[65];

    struct Block *next;
} Block;

typedef struct {
    char tx_id[65];
    char student_id[20];
    int amount;          // 10, 5, 0
    int fee;             // fixed later (e.g. 1)
    time_t timestamp;
} Transaction;

Transaction pending_pool[MAX_PENDING];


void view_pending_transactions() {

    printf("\n===== PENDING TRANSACTIONS =====\n");

    for (int i = 0; i < pending_count; i++) {
        printf("\nTX ID: %s\n", pending_pool[i].tx_id);
        printf("Student ID: %s\n", pending_pool[i].student_id);
        printf("Amount: %d\n", pending_pool[i].amount);
        printf("Fee: %d\n", pending_pool[i].fee);
        printf("Timestamp: %ld\n", pending_pool[i].timestamp);
    }
}

void calculate_tx_hash(Transaction *tx, char output[65]) {
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    char data[256];

    sprintf(data, "%s%d%d%ld",
        tx->student_id,
        tx->amount,
        tx->fee,
        tx->timestamp
    );

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Update(&sha256, data, strlen(data));
    SHA256_Final(hash, &sha256);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }

    output[64] = '\0';
}

void create_transaction(char *student_id, int amount) {

    if (pending_count >= MAX_PENDING) {
        printf("Pending pool full!\n");
        return;
    }

    Transaction tx;

    strcpy(tx.student_id, student_id);
    tx.amount = amount;
    tx.fee = 1;
    tx.timestamp = time(NULL);

    calculate_tx_hash(&tx, tx.tx_id);

    pending_pool[pending_count++] = tx;

    printf("\nTransaction created and added to pending pool.\n");
    printf("TX_ID: %s\n", tx.tx_id);
}

Student students[MAX_STUDENTS];
int student_count = 0;

Block *blockchain = NULL;

EC_KEY *ecdsa_key;

/* ========================= */
/* STUDENT REGISTRY FUNCTIONS */
/* ========================= */

int load_students() {

    FILE *file = fopen("students.txt", "r");

    if (!file) {
        printf("ERROR: students.txt missing\n");
        return 0;
    }

    char line[128];

    while (fgets(line, sizeof(line), file)) {

        if (sscanf(
            line,
            "%[^,],%[^,],%s",
            students[student_count].student_id,
            students[student_count].full_name,
            students[student_count].course_code
        ) == 3) {

            student_count++;
        }
    }

    fclose(file);

    if (student_count == 0) {
        printf("ERROR: students.txt empty\n");
        return 0;
    }

    return 1;
}

Student* find_student(char *id) {

    for (int i = 0; i < student_count; i++) {

        if (strcmp(students[i].student_id, id) == 0) {
            return &students[i];
        }
    }

    return NULL;
}

/* ========================= */
/* HASHING FUNCTIONS */
/* ========================= */

void calculate_hash(Block *block, char output[65]) {

    SHA256_CTX sha256;

    SHA256_Init(&sha256);

    char data[512];

    sprintf(
        data,
        "%d%ld%s%s%s%s%s",
        block->index,
        block->timestamp,
        block->student_id,
        block->full_name,
        block->course_code,
        block->status,
        block->previous_hash
    );

    SHA256_Update(&sha256, data, strlen(data));

    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256_Final(hash, &sha256);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }

    output[64] = '\0';
}

/* ========================= */
/* DIGITAL SIGNATURES */
/* ========================= */

void generate_keys() {

    ecdsa_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

    if (!ecdsa_key) {
        printf("Key creation failed\n");
        exit(1);
    }

    if (!EC_KEY_generate_key(ecdsa_key)) {
        printf("Key generation failed\n");
        exit(1);
    }
}

void sign_block(Block *block) {

    if (!ECDSA_sign(
        0,
        (unsigned char*)block->hash,
        strlen(block->hash),
        block->signature,
        &block->signature_length,
        ecdsa_key
    )) {

        printf("Signing failed\n");
        exit(1);
    }
}

int verify_signature(Block *block) {

    return ECDSA_verify(
        0,
        (unsigned char*)block->hash,
        strlen(block->hash),
        block->signature,
        block->signature_length,
        ecdsa_key
    );
}

/* ========================= */
/* BLOCKCHAIN FUNCTIONS */
/* ========================= */

Block* create_genesis_block() {

    Block *block = (Block*)malloc(sizeof(Block));

    block->index = 0;

    block->timestamp = time(NULL);

    strcpy(block->student_id, "GENESIS");
    strcpy(block->full_name, "GENESIS BLOCK");
    strcpy(block->course_code, "NONE");
    strcpy(block->status, "NONE");

    memset(block->previous_hash, '0', 64);
    block->previous_hash[64] = '\0';

    calculate_hash(block, block->hash);

    sign_block(block);

    block->next = NULL;

    return block;
}

Block* get_last_block() {

    Block *current = blockchain;

    while (current->next != NULL) {
        current = current->next;
    }

    return current;
}

void add_block(Student *student, char *status) {

    Block *previous = get_last_block();

    Block *new_block = (Block*)malloc(sizeof(Block));

    new_block->index = previous->index + 1;

    new_block->timestamp = time(NULL);

    strcpy(new_block->student_id, student->student_id);
    strcpy(new_block->full_name, student->full_name);
    strcpy(new_block->course_code, student->course_code);
    strcpy(new_block->status, status);

    strcpy(new_block->previous_hash, previous->hash);

    calculate_hash(new_block, new_block->hash);

    sign_block(new_block);

    new_block->next = NULL;

    previous->next = new_block;

    printf("\nAttendance recorded successfully.\n");
}

void view_blockchain() {

    Block *current = blockchain;

    printf("\n===== ATTENDANCE RECORDS =====\n");

    while (current != NULL) {

        printf("\nIndex: %d\n", current->index);
        printf("Student ID: %s\n", current->student_id);
        printf("Name: %s\n", current->full_name);
        printf("Course: %s\n", current->course_code);
        printf("Status: %s\n", current->status);
        printf("Timestamp: %ld\n", current->timestamp);
        printf("Hash: %s\n", current->hash);
        printf("Previous Hash: %s\n", current->previous_hash);

        if (verify_signature(current)) {
            printf("Signature: VALID\n");
        } else {
            printf("Signature: INVALID\n");
        }

        current = current->next;
    }
}

int validate_chain() {

    Block *current = blockchain;

    while (current->next != NULL) {

        char recalculated_hash[65];

        calculate_hash(current, recalculated_hash);

        if (strcmp(current->hash, recalculated_hash) != 0) {

            printf("\nHash mismatch detected at block %d\n", current->index);

            return 0;
        }

        if (strcmp(current->next->previous_hash, current->hash) != 0) {

            printf("\nBroken chain linkage detected\n");

            return 0;
        }

        if (!verify_signature(current)) {

            printf("\nInvalid digital signature detected\n");

            return 0;
        }

        current = current->next;
    }

    printf("\nBlockchain is VALID\n");

    return 1;
}

/* ========================= */
/* FILE STORAGE */
/* ========================= */

void save_blockchain() {

    FILE *file = fopen("attendance.dat", "wb");

    if (!file) {
        printf("Could not save blockchain\n");
        return;
    }

    Block *current = blockchain;

    while (current != NULL) {

        fwrite(current, sizeof(Block), 1, file);

        current = current->next;
    }

    fclose(file);

    printf("Blockchain saved successfully\n");
}

/* ========================= */
/* TAMPERING DEMO */
/* ========================= */

void tamper_demo() {

    if (blockchain->next == NULL) {

        printf("\nNeed at least one attendance record.\n");

        return;
    }

    printf("\nTampering with block 1...\n");

    strcpy(blockchain->next->status, "ABSENT");

    calculate_hash(blockchain->next, blockchain->next->hash);

    sign_block(blockchain->next);

    printf("Block modified.\n");

    validate_chain();
}

/* ========================= */
/* MENU */
/* ========================= */

void menu() {

    int choice;

    char id[20];
    char status[10];

    while (1) {

        printf("\n===== BLOCKCHAIN ATTENDANCE SYSTEM =====\n");

        printf("1. Mark Attendance\n");
        printf("2. View Records\n");
        printf("3. Validate Blockchain\n");
        printf("4. Demonstrate Tampering\n");
        printf("5. View Pending Transactions\n");
        printf("6. Exit\n");

        printf("\nEnter choice: ");
        scanf("%d", &choice);

        switch (choice) {

            case 1: {

                printf("\nEnter Student ID: ");
                scanf("%s", id);

                Student *student = find_student(id);

                if (!student) {

                    printf("ERROR: Student ID not found\n");

                    break;
                }

                printf("Enter Status (PRESENT/LATE/ABSENT): ");
                scanf("%s", status);

                if (strcmp(status, "PRESENT") == 0) {
                    create_transaction(student->student_id, 10);
                }
                else if (strcmp(status, "LATE") == 0) {
                    create_transaction(student->student_id, 5);
                }
                else {
                    printf("No transaction created for ABSENT\n");
                }

                break;
            }

            case 2:
                view_blockchain();
                break;

            case 3:
                validate_chain();
                break;

            case 4:
                tamper_demo();
                break;


            case 5:
                view_pending_transactions();
                break;

            case 6:
                printf("\nExiting...\n");
                exit(0);

            default:
                printf("\nInvalid choice\n");
        }
    }
}

/* ========================= */
/* MAIN */
/* ========================= */

int main() {

    printf("Loading student registry...\n");

    if (!load_students()) {
        return 1;
    }

    printf("Loaded %d students successfully\n", student_count);

    generate_keys();

    blockchain = create_genesis_block();

    menu();

    return 0;
}
