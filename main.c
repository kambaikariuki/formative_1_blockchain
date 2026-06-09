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
#define MAX_TX_IN_BLOCK 50
#define MAX_UTXOS 1000


int pending_count = 0;
typedef struct {
    char student_id[20];
    char full_name[50];
    char course_code[10];
} Student;

typedef struct {
    char tx_id[65];
    char student_id[20];

    char sender[20];
    char recipient[20];

    int input_count;
    int inputs[10];
    int amount;
    int fee; 
    time_t timestamp;
} Transaction;

typedef struct Block {
    int index;
    time_t timestamp;

    char previous_hash[65];

    Transaction transactions[MAX_TX_IN_BLOCK];
    int tx_count;

    char hash[65];

    unsigned int nonce;   // NEW for mining

    struct Block *next;
} Block;

Block *blockchain = NULL;
Block* get_last_block();

Transaction pending_pool[MAX_PENDING];

typedef struct {
    char tx_id[65];
    int output_index;

    char owner[20];

    int amount;

    int spent;
} UTXO;

UTXO utxo_set[MAX_UTXOS];
int utxo_count = 0;

void create_reward_utxo(Transaction *tx)
{
    if (tx->amount <= tx->fee)
        return;
    printf("debug a");
    UTXO *u = &utxo_set[utxo_count++];

    printf("debug a");
    strcpy(u->tx_id, tx->tx_id);
    u->output_index = 0;
    printf("debug a");
    strcpy(u->owner, tx->student_id);
    printf("debug a");
    u->amount = tx->amount - tx->fee;
    u->spent = 0;
    printf("debug a");
}

void calculate_tx_hash(Transaction *tx, char output[65]);
void calculate_hash(Block *block, char output[65]);

void mine_block(Block *block) {

    block->nonce = 0;

    char hash[65];

    while (1) {

        calculate_hash(block, hash);

        // Simple difficulty: first 4 chars must be "0000"
        if (strncmp(hash, "0000", 4) == 0) {
            strcpy(block->hash, hash);
            break;
        }

        block->nonce++;
    }
    printf("deeebug1");
    printf("\nBlock mined successfully with nonce %u\n", block->nonce);
    printf("deeebug2");
    for (int i = 0; i < block->tx_count; i++)
    {   
        printf("deeebug");
        create_reward_utxo(&block->transactions[i]);
    }
}

int get_balance(char *student_id)
{
    int balance = 0;

    for (int i = 0; i < utxo_count; i++)
    {
        if (!utxo_set[i].spent &&
            strcmp(utxo_set[i].owner, student_id) == 0)
        {
            balance += utxo_set[i].amount;
        }
    }

    return balance;
}

void display_utxo_set()
{
    printf("\n===== UTXO SET =====\n");

    for (int i = 0; i < utxo_count; i++)
    {
        if (!utxo_set[i].spent)
        {
            printf(
                "TX:%s Owner:%s Amount:%d\n",
                utxo_set[i].tx_id,
                utxo_set[i].owner,
                utxo_set[i].amount
            );
        }
    }
}

int select_utxos(char *sender, int needed, int *selected, int *selected_count)
{
    int total = 0;
    *selected_count = 0;

    for (int i = 0; i < utxo_count; i++)
    {
        if (!utxo_set[i].spent &&
            strcmp(utxo_set[i].owner, sender) == 0)
        {
            selected[(*selected_count)++] = i;
            total += utxo_set[i].amount;

            if (total >= needed)
                return total;
        }
    }

    return total;
}

void create_transfer_transaction(char *sender, char *recipient, int amount)
{
    int fee = 1;
    int needed = amount + fee;

    int selected[10];
    int selected_count = 0;

    int total = select_utxos(sender, needed, selected, &selected_count);

    if (total < needed)
    {
        printf("Insufficient balance\n");
        return;
    }

    Transaction tx;
    strcpy(tx.sender, sender);
    strcpy(tx.recipient, recipient);
    tx.amount = amount;
    tx.fee = fee;

    tx.input_count = selected_count;

    for (int i = 0; i < selected_count; i++)
        tx.inputs[i] = selected[i];

    calculate_tx_hash(&tx, tx.tx_id);

    pending_pool[pending_count++] = tx;

    printf("Transaction created and added to pending pool\n");
}

void apply_transaction(Transaction *tx)
{
    int total_in = 0;

    // 1. Mark inputs as spent
    for (int i = 0; i < tx->input_count; i++)
    {
        int idx = tx->inputs[i];

        if (utxo_set[idx].spent)
        {
            printf("Double spend detected!\n");
            return;
        }

        utxo_set[idx].spent = 1;
        total_in += utxo_set[idx].amount;
    }

    int total_out = tx->amount + tx->fee;

    // 2. Create recipient UTXO
    if (utxo_count >= MAX_UTXOS) {
        printf("UTXO overflow (recipient)\n");
        return;
    }
    UTXO *u1 = &utxo_set[utxo_count++];
    
    strcpy(u1->owner, tx->recipient);
    strcpy(u1->tx_id, tx->tx_id);
    u1->amount = tx->amount;
    u1->spent = 0;

    // 3. Create change UTXO (if any)
    int change = total_in - total_out;

    if (change > 0)
    {
        UTXO *u2 = &utxo_set[utxo_count++];
        strcpy(u2->owner, tx->sender);
        strcpy(u2->tx_id, tx->tx_id);
        u2->amount = change;
        u2->spent = 0;
    }
}

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

Block* get_last_block() {

    if (blockchain == NULL)
        return NULL;

    Block *current = blockchain;

    while (current->next != NULL)
        current = current->next;

    return current;
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

    char data[1024];
    char tx_data[4096] = "";

    for (int i = 0; i < block->tx_count; i++) {
        char temp[128];
        sprintf(temp, "%s%d%ld",
            block->transactions[i].student_id,
            block->transactions[i].amount,
            block->transactions[i].timestamp
        );
        strncat(
            tx_data,
            temp,
            sizeof(tx_data) - strlen(tx_data) - 1
        );
    }

    sprintf(data, "%d%ld%s%u%s",
        block->index,
        block->timestamp,
        block->previous_hash,
        block->nonce,
        tx_data
    );

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Update(&sha256, data, strlen(data));
    SHA256_Final(hash, &sha256);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }

    output[64] = '\0';
}

void mine_pending_transactions() {

    if (pending_count == 0) {
        printf("\nNo transactions to mine.\n");
        return;
    }

    Block *block = (Block*)malloc(sizeof(Block));
    if (!block) {
        printf("Memory allocation failed\n");
        return;
    }

    block->timestamp = time(NULL);
    block->tx_count = 0;
    block->nonce = 0;

    // SAFE: handle empty chain FIRST
    if (blockchain == NULL) {
        block->index = 0;
        strcpy(block->previous_hash, "0");
    } else {
        Block *last = get_last_block();
        if (!last) {
            printf("Chain corruption detected\n");
            return;
        }

        block->index = last->index + 1;
        strcpy(block->previous_hash, last->hash);
    }

    // Copy transactions safely
    for (int i = 0; i < pending_count && i < MAX_TX_IN_BLOCK; i++) {
        block->transactions[i] = pending_pool[i];
        block->tx_count++;
    }

    printf("\nMining block with %d transactions...\n", block->tx_count);

    printf("1");
    mine_block(block);

    printf("2");
    // Apply transactions AFTER mining
    for (int i = 0; i < block->tx_count; i++) {
        apply_transaction(&block->transactions[i]);
    }

    // Attach block safely
    block->next = NULL;
    printf("3");
    if (blockchain == NULL) {
        blockchain = block;
    } else {
        Block *last = get_last_block();
        printf("4");
        last->next = block;
    }

    printf("5");
    // Clear pending pool safely
    pending_count = 0;

    printf("\nBlock added to blockchain!\n");
}

/* ========================= */
/* BLOCKCHAIN FUNCTIONS */
/* ========================= */

Block* create_genesis_block() {

    Block *block = malloc(sizeof(Block));

    block->index = 0;
    block->timestamp = time(NULL);

    strcpy(block->previous_hash, "0");

    block->nonce = 0;
    block->tx_count = 0;

    calculate_hash(block, block->hash);

    block->next = NULL;

    return block;
}


// void add_block(Student *student, char *status) {
//
//     Block *previous = get_last_block();
//
//     Block *new_block = (Block*)malloc(sizeof(Block));
//
//     new_block->index = previous->index + 1;
//
//     new_block->timestamp = time(NULL);
//
//     strcpy(new_block->student_id, student->student_id);
//     strcpy(new_block->full_name, student->full_name);
//     strcpy(new_block->course_code, student->course_code);
//     strcpy(new_block->status, status);
//
//     strcpy(new_block->previous_hash, previous->hash);
//
//     calculate_hash(new_block, new_block->hash);
//
//     sign_block(new_block);
//
//     new_block->next = NULL;
//
//     previous->next = new_block;
//
//     printf("\nAttendance recorded successfully.\n");
// }

void view_blockchain() {

    Block *current = blockchain;

    printf("\n===== BLOCKCHAIN =====\n");

    while (current != NULL) {

        printf("\nIndex: %d\n", current->index);
        printf("Timestamp: %ld\n", current->timestamp);
        printf("Previous Hash: %s\n", current->previous_hash);
        printf("Hash: %s\n", current->hash);
        printf("Nonce: %u\n", current->nonce);
        printf("Transactions: %d\n", current->tx_count);

        for (int i = 0; i < current->tx_count; i++) {
            printf("  TX %d: %s | %d\n",
                i,
                current->transactions[i].student_id,
                current->transactions[i].amount
            );
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

    strcpy(blockchain->next->transactions[0].student_id, "HACKED");

    calculate_hash(blockchain->next, blockchain->next->hash);


    printf("Block modified.\n");

    validate_chain();
}

/* ========================= */
/* MENU */
/* ========================= */

void menu() {

    int choice;

    char id[20];
    char recipient[20];
    int amount;

    while (1) {

        printf("\n===== BLOCKCHAIN ATTENDANCE + UTXO SYSTEM =====\n");
        printf("1. Mark Attendance (Create Transaction)\n");
        printf("2. Mine Pending Transactions\n");
        printf("3. View Blockchain\n");
        printf("4. View Pending Transactions\n");
        printf("5. View UTXO Set\n");
        printf("6. Transfer Tokens (UTXO Spend)\n");
        printf("7. Check Student Balance\n");
        printf("8. Exit\n");

        printf("\nEnter choice: ");
        scanf("%d", &choice);

        switch (choice) {

            case 1: {
                printf("\nEnter Student ID: ");
                scanf("%s", id);

                Student *student = find_student(id);

                if (!student) {
                    printf("Student not found\n");
                    break;
                }

                printf("Enter Status (PRESENT/LATE/ABSENT): ");
                char status[10];
                scanf("%s", status);

                if (strcmp(status, "PRESENT") == 0) {
                    create_transaction(student->student_id, 10);
                }
                else if (strcmp(status, "LATE") == 0) {
                    create_transaction(student->student_id, 5);
                }
                else {
                    printf("No transaction created\n");
                }

                break;
            }

            case 2:
                mine_pending_transactions();
                break;

            case 3:
                view_blockchain();
                break;

            case 4:
                view_pending_transactions();
                break;

            case 5:
                display_utxo_set();
                break;

            case 6: {
                printf("\nEnter sender ID: ");
                scanf("%s", id);

                printf("Enter recipient ID: ");
                scanf("%s", recipient);

                printf("Enter amount: ");
                scanf("%d", &amount);

                create_transfer_transaction(id, recipient, amount);

                break;
            }

            case 7: {
                printf("\nEnter Student ID: ");
                scanf("%s", id);

                printf("Balance: %d\n", get_balance(id));

                break;
            }

            case 8:
                printf("\nExiting...\n");
                return;

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

    blockchain = create_genesis_block();

    menu();

    return 0;
}
