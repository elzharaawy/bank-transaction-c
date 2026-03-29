/*
 * transaction.c - Simple Bank Transaction System in C
 * Features: deposit, withdraw, transfer, and transaction history
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─── Constants ─────────────────────────────────────────────── */
#define MAX_ACCOUNTS     10
#define MAX_HISTORY      50
#define ACCOUNT_ID_LEN   10

/* ─── Data Structures ───────────────────────────────────────── */

typedef enum {
    DEPOSIT,
    WITHDRAW,
    TRANSFER_OUT,
    TRANSFER_IN
} TransactionType;

typedef struct {
    TransactionType type;
    double          amount;
    double          balance_after;
    char            description[64];
    time_t          timestamp;
} Transaction;

typedef struct {
    char        id[ACCOUNT_ID_LEN];
    char        owner[32];
    double      balance;
    Transaction history[MAX_HISTORY];
    int         history_count;
} Account;

/* ─── Global State ──────────────────────────────────────────── */
Account accounts[MAX_ACCOUNTS];
int     account_count = 0;

/* ─── Helpers ───────────────────────────────────────────────── */

Account *find_account(const char *id) {
    for (int i = 0; i < account_count; i++)
        if (strcmp(accounts[i].id, id) == 0)
            return &accounts[i];
    return NULL;
}

void log_transaction(Account *acc, TransactionType type,
                     double amount, const char *desc) {
    if (acc->history_count >= MAX_HISTORY) {
        /* Shift out the oldest entry */
        memmove(&acc->history[0], &acc->history[1],
                sizeof(Transaction) * (MAX_HISTORY - 1));
        acc->history_count = MAX_HISTORY - 1;
    }
    Transaction *t = &acc->history[acc->history_count++];
    t->type          = type;
    t->amount        = amount;
    t->balance_after = acc->balance;
    t->timestamp     = time(NULL);
    strncpy(t->description, desc, sizeof(t->description) - 1);
}

const char *type_str(TransactionType t) {
    switch (t) {
        case DEPOSIT:      return "DEPOSIT     ";
        case WITHDRAW:     return "WITHDRAW    ";
        case TRANSFER_OUT: return "TRANSFER OUT";
        case TRANSFER_IN:  return "TRANSFER IN ";
        default:           return "UNKNOWN     ";
    }
}

/* ─── Core Operations ───────────────────────────────────────── */

int create_account(const char *id, const char *owner, double initial) {
    if (account_count >= MAX_ACCOUNTS) {
        printf("ERROR: Maximum account limit reached.\n");
        return 0;
    }
    if (find_account(id)) {
        printf("ERROR: Account '%s' already exists.\n", id);
        return 0;
    }
    Account *acc = &accounts[account_count++];
    strncpy(acc->id,    id,    ACCOUNT_ID_LEN - 1);
    strncpy(acc->owner, owner, sizeof(acc->owner) - 1);
    acc->balance       = initial;
    acc->history_count = 0;

    if (initial > 0)
        log_transaction(acc, DEPOSIT, initial, "Initial deposit");

    printf("✓ Account created  [%s] Owner: %s  Balance: $%.2f\n",
           acc->id, acc->owner, acc->balance);
    return 1;
}

int deposit(const char *id, double amount) {
    if (amount <= 0) { printf("ERROR: Deposit amount must be positive.\n"); return 0; }
    Account *acc = find_account(id);
    if (!acc) { printf("ERROR: Account '%s' not found.\n", id); return 0; }

    acc->balance += amount;
    log_transaction(acc, DEPOSIT, amount, "Cash deposit");
    printf("✓ Deposited $%.2f → [%s]  New balance: $%.2f\n",
           amount, id, acc->balance);
    return 1;
}

int withdraw(const char *id, double amount) {
    if (amount <= 0) { printf("ERROR: Withdraw amount must be positive.\n"); return 0; }
    Account *acc = find_account(id);
    if (!acc) { printf("ERROR: Account '%s' not found.\n", id); return 0; }
    if (acc->balance < amount) {
        printf("ERROR: Insufficient funds. Balance: $%.2f  Requested: $%.2f\n",
               acc->balance, amount);
        return 0;
    }
    acc->balance -= amount;
    log_transaction(acc, WITHDRAW, amount, "Cash withdrawal");
    printf("✓ Withdrew  $%.2f ← [%s]  New balance: $%.2f\n",
           amount, id, acc->balance);
    return 1;
}

int transfer(const char *from_id, const char *to_id, double amount) {
    if (amount <= 0) { printf("ERROR: Transfer amount must be positive.\n"); return 0; }
    if (strcmp(from_id, to_id) == 0) {
        printf("ERROR: Cannot transfer to the same account.\n"); return 0;
    }
    Account *src = find_account(from_id);
    Account *dst = find_account(to_id);
    if (!src) { printf("ERROR: Source account '%s' not found.\n",      from_id); return 0; }
    if (!dst) { printf("ERROR: Destination account '%s' not found.\n", to_id);   return 0; }
    if (src->balance < amount) {
        printf("ERROR: Insufficient funds. Balance: $%.2f  Requested: $%.2f\n",
               src->balance, amount);
        return 0;
    }

    src->balance -= amount;
    dst->balance += amount;

    char desc[64];
    snprintf(desc, sizeof(desc), "Transfer to [%s]",   to_id);
    log_transaction(src, TRANSFER_OUT, amount, desc);
    snprintf(desc, sizeof(desc), "Transfer from [%s]", from_id);
    log_transaction(dst, TRANSFER_IN,  amount, desc);

    printf("✓ Transferred $%.2f  [%s] → [%s]\n", amount, from_id, to_id);
    printf("  %s balance: $%.2f    %s balance: $%.2f\n",
           from_id, src->balance, to_id, dst->balance);
    return 1;
}

void print_history(const char *id) {
    Account *acc = find_account(id);
    if (!acc) { printf("ERROR: Account '%s' not found.\n", id); return; }

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Transaction History  |  Account: %s  (%s)\n", acc->id, acc->owner);
    printf("  Current Balance: $%.2f\n", acc->balance);
    printf("══════════════════════════════════════════════════\n");
    printf("  %-12s  %10s  %12s  %s\n", "TYPE", "AMOUNT", "BALANCE", "DESCRIPTION");
    printf("  ──────────────────────────────────────────────\n");

    if (acc->history_count == 0) {
        printf("  (no transactions)\n");
    } else {
        for (int i = 0; i < acc->history_count; i++) {
            Transaction *t = &acc->history[i];
            printf("  %-12s  %10.2f  %12.2f  %s\n",
                   type_str(t->type), t->amount,
                   t->balance_after, t->description);
        }
    }
    printf("══════════════════════════════════════════════════\n\n");
}

void list_accounts(void) {
    printf("\n┌─────────────────────────────────────────────┐\n");
    printf("│              All Accounts                   │\n");
    printf("├──────────┬──────────────────┬───────────────┤\n");
    printf("│ ID       │ Owner            │ Balance       │\n");
    printf("├──────────┼──────────────────┼───────────────┤\n");
    for (int i = 0; i < account_count; i++) {
        printf("│ %-8s │ %-16s │ %13.2f │\n",
               accounts[i].id, accounts[i].owner, accounts[i].balance);
    }
    printf("└──────────┴──────────────────┴───────────────┘\n\n");
}

/* ─── Interactive Menu ──────────────────────────────────────── */

void menu(void) {
    int    choice;
    char   id1[ACCOUNT_ID_LEN], id2[ACCOUNT_ID_LEN], owner[32];
    double amount;

    while (1) {
        printf("\n╔══════════════════════════════╗\n");
        printf("║   BANK TRANSACTION SYSTEM    ║\n");
        printf("╠══════════════════════════════╣\n");
        printf("║ 1. Create Account            ║\n");
        printf("║ 2. Deposit                   ║\n");
        printf("║ 3. Withdraw                  ║\n");
        printf("║ 4. Transfer                  ║\n");
        printf("║ 5. View Transaction History  ║\n");
        printf("║ 6. List All Accounts         ║\n");
        printf("║ 0. Exit                      ║\n");
        printf("╚══════════════════════════════╝\n");
        printf("Choice: ");
        if (scanf("%d", &choice) != 1) { while(getchar()!='\n'); continue; }

        switch (choice) {
            case 1:
                printf("Account ID: ");   scanf("%9s",  id1);
                printf("Owner name: ");  scanf("%31s", owner);
                printf("Initial deposit: $"); scanf("%lf", &amount);
                create_account(id1, owner, amount);
                break;
            case 2:
                printf("Account ID: ");  scanf("%9s",  id1);
                printf("Amount: $");     scanf("%lf", &amount);
                deposit(id1, amount);
                break;
            case 3:
                printf("Account ID: ");  scanf("%9s",  id1);
                printf("Amount: $");     scanf("%lf", &amount);
                withdraw(id1, amount);
                break;
            case 4:
                printf("From Account ID: "); scanf("%9s",  id1);
                printf("To   Account ID: "); scanf("%9s",  id2);
                printf("Amount: $");         scanf("%lf", &amount);
                transfer(id1, id2, amount);
                break;
            case 5:
                printf("Account ID: "); scanf("%9s", id1);
                print_history(id1);
                break;
            case 6:
                list_accounts();
                break;
            case 0:
                printf("Goodbye!\n");
                return;
            default:
                printf("Invalid choice. Try again.\n");
        }
    }
}

/* ─── Main ──────────────────────────────────────────────────── */

int main(void) {
    /* Seed a few demo accounts */
    create_account("ACC001", "Alice",  1000.00);
    create_account("ACC002", "Bob",     500.00);
    create_account("ACC003", "Charlie", 250.00);

    /* Run a quick demo before the interactive menu */
    printf("\n--- Demo transactions ---\n");
    deposit("ACC001",  200.00);
    withdraw("ACC002", 150.00);
    transfer("ACC001", "ACC003", 300.00);
    print_history("ACC001");

    /* Hand off to the interactive menu */
    menu();
    return 0;
}