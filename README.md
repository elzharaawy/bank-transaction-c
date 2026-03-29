# Bank Transaction System in C

A console-based bank transaction system written in C.  
Supports account creation, deposits, withdrawals, transfers, and full transaction history.

---

## Table of contents

1. [Features](#features)
2. [Getting started](#getting-started)
3. [System design](#system-design)
4. [Data structures](#data-structures)
5. [How each operation works](#how-each-operation-works)
6. [Transaction flow diagram](#transaction-flow-diagram)
7. [Code walkthrough](#code-walkthrough)
8. [Error handling](#error-handling)
9. [Limitations](#limitations)
10. [Future improvements](#future-improvements)

---

## Features

| Feature             | Description                                              |
|---------------------|----------------------------------------------------------|
| Create account      | Register an account with ID, owner name, initial balance |
| Deposit             | Add funds to any account                                 |
| Withdraw            | Remove funds (with balance check)                        |
| Transfer            | Move funds between two accounts atomically               |
| Transaction history | View timestamped ledger for any account                  |
| List all accounts   | Display a table of all accounts and balances             |

---

## Getting started

### Prerequisites

- GCC or any C99-compatible compiler
- Terminal / command prompt

### Compile

```bash
gcc -o transaction transaction.c
```

### Run

```bash
./transaction
```

On Windows:

```bash
gcc -o transaction.exe transaction.c
transaction.exe
```

---

## System design

```
┌─────────────────────────────────────────────────────┐
│                  Interactive menu                    │  ← UI layer
│         (main loop, scanf, switch-case)              │
└──────────┬──────────┬──────────┬──────────┬─────────┘
           │          │          │          │
    create_account  deposit  withdraw   transfer        ← Core operations
           │          │          │          │
     ┌─────┴──────────┴──────────┴──────────┴─────┐
     │   find_account()   log_transaction()        │    ← Helpers
     └─────────────────────────────┬───────────────┘
                                   │
                    accounts[MAX_ACCOUNTS]               ← Global data store
                    history[MAX_HISTORY] per account
```

The program follows a simple **layered architecture**:

- **UI layer** — the `menu()` function handles all input/output and dispatches user choices.
- **Operations layer** — each of `deposit`, `withdraw`, `transfer`, `create_account` is a self-contained function that validates input, mutates state, and calls helpers.
- **Helper layer** — `find_account` and `log_transaction` are shared utilities used by every operation.
- **Data layer** — a global array of `Account` structs that holds all state in memory.

---

## Data structures

### `TransactionType` (enum)

```c
typedef enum {
    DEPOSIT,
    WITHDRAW,
    TRANSFER_OUT,
    TRANSFER_IN
} TransactionType;
```

Describes what kind of movement a transaction record represents.

---

### `Transaction` (struct)

```c
typedef struct {
    TransactionType type;        // what kind of transaction
    double          amount;      // how much money moved
    double          balance_after; // account balance after this event
    char            description[64]; // human-readable note
    time_t          timestamp;   // Unix timestamp (from time.h)
} Transaction;
```

Each transaction is a **snapshot** — it records not just the amount but the resulting balance, so you can reconstruct the full ledger at any point.

---

### `Account` (struct)

```c
typedef struct {
    char        id[ACCOUNT_ID_LEN];      // unique identifier, e.g. "ACC001"
    char        owner[32];               // owner's name
    double      balance;                 // current balance
    Transaction history[MAX_HISTORY];    // ring buffer of past transactions
    int         history_count;           // how many entries are used
} Account;
```

All accounts live in a single global array:

```c
Account accounts[MAX_ACCOUNTS];   // max 10 accounts
int     account_count = 0;        // how many are active
```

---

## How each operation works

### `create_account(id, owner, initial)`

1. Check that `account_count < MAX_ACCOUNTS`
2. Check that no account with this `id` already exists (`find_account`)
3. Copy `id`, `owner`, and set `balance = initial`
4. If `initial > 0`, log an initial deposit transaction
5. Increment `account_count`

---

### `deposit(id, amount)`

```
validate amount > 0
  └── find account by id
        └── acc->balance += amount
              └── log_transaction(DEPOSIT, ...)
```

---

### `withdraw(id, amount)`

```
validate amount > 0
  └── find account by id
        └── check acc->balance >= amount
              └── acc->balance -= amount
                    └── log_transaction(WITHDRAW, ...)
```

If the balance is insufficient, the withdrawal is **rejected** and the account is unchanged.

---

### `transfer(from_id, to_id, amount)`

Both accounts are looked up **before** any mutation:

```
validate amount > 0 and from_id != to_id
  └── find src account
  └── find dst account
        └── check src->balance >= amount
              └── src->balance -= amount
              └── dst->balance += amount
              └── log TRANSFER_OUT on src
              └── log TRANSFER_IN  on dst
```

Both debit and credit happen in the same function call, making the transfer effectively **atomic** (no partial state is possible within a single-threaded program).

---

### `log_transaction(acc, type, amount, desc)`

Adds one `Transaction` entry to `acc->history[]`.  
If the history buffer is full (`history_count >= MAX_HISTORY`), it **shifts** all entries left by one using `memmove`, discarding the oldest, and writes the new entry at the end — a sliding window / ring buffer pattern.

---

## Transaction flow diagram

```
User input
    │
    ▼
menu() ──── 1 ──► create_account()
    │
    ├─── 2 ──► deposit()    ──┐
    │                         │
    ├─── 3 ──► withdraw()   ──┼──► find_account()
    │                         │         │
    ├─── 4 ──► transfer()  ───┘         ▼
    │                              accounts[]
    ├─── 5 ──► print_history()         │
    │                             log_transaction()
    └─── 6 ──► list_accounts()         │
                                   history[]
```

---

## Code walkthrough

### Entry point — `main()`

```c
int main(void) {
    // 1. Pre-seed three demo accounts
    create_account("ACC001", "Alice",  1000.00);
    create_account("ACC002", "Bob",     500.00);
    create_account("ACC003", "Charlie", 250.00);

    // 2. Run a quick demo
    deposit("ACC001",  200.00);
    withdraw("ACC002", 150.00);
    transfer("ACC001", "ACC003", 300.00);
    print_history("ACC001");

    // 3. Hand off to interactive mode
    menu();
    return 0;
}
```

The demo runs automatically on startup so you can see the system working before typing anything.

---

### Finding an account — `find_account()`

```c
Account *find_account(const char *id) {
    for (int i = 0; i < account_count; i++)
        if (strcmp(accounts[i].id, id) == 0)
            return &accounts[i];
    return NULL;
}
```

Returns a **pointer** to the matching account so the caller can mutate it directly. Returns `NULL` if not found — callers must always check this.

---

### Logging — `log_transaction()`

```c
void log_transaction(Account *acc, TransactionType type,
                     double amount, const char *desc) {
    if (acc->history_count >= MAX_HISTORY) {
        memmove(&acc->history[0], &acc->history[1],
                sizeof(Transaction) * (MAX_HISTORY - 1));
        acc->history_count = MAX_HISTORY - 1;
    }
    Transaction *t = &acc->history[acc->history_count++];
    t->type          = type;
    t->amount        = amount;
    t->balance_after = acc->balance;  // captured AFTER mutation
    t->timestamp     = time(NULL);
    strncpy(t->description, desc, sizeof(t->description) - 1);
}
```

`balance_after` is captured **after** the caller has already updated `acc->balance`, so the history is always consistent.

---

## Error handling

| Condition                    | Response                                      |
|------------------------------|-----------------------------------------------|
| Account not found            | Prints `ERROR:` message, returns `0`          |
| Duplicate account ID         | Prints `ERROR:` message, returns `0`          |
| Insufficient funds           | Prints `ERROR:` with balance vs requested     |
| Amount ≤ 0                   | Prints `ERROR: amount must be positive`       |
| Transfer to same account     | Prints `ERROR: cannot transfer to same account` |
| Account limit reached        | Prints `ERROR: maximum account limit reached` |

All operations return `1` on success and `0` on failure. The caller (menu) ignores the return value — but it is there for future use.

---

## Limitations

- **In-memory only** — all data is lost when the program exits. No file or database persistence.
- **No concurrency** — not thread-safe. Designed for single-user, single-threaded use.
- **Fixed limits** — `MAX_ACCOUNTS = 10`, `MAX_HISTORY = 50` are compile-time constants.
- **No authentication** — any user can access any account.
- **`double` for currency** — floating-point arithmetic can produce tiny rounding errors. A production system would use integer cents or a fixed-point type.

---

## Future improvements

- [ ] Save/load accounts to a binary or CSV file for persistence
- [ ] Add PIN / password per account
- [ ] Use `long long` integer cents instead of `double` to avoid floating-point drift
- [ ] Expand `MAX_ACCOUNTS` dynamically with `malloc` / `realloc`
- [ ] Add a search / filter function for transaction history
- [ ] Unit tests with a test harness (e.g. Unity or a simple custom test runner)

---

## File structure

```
.
├── transaction.c   # all source code (single-file project)
└── README.md       # this file
```

---

## License

MIT — free to use, modify, and distribute.
