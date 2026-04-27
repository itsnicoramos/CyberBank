# CyberBank

A two part banking simulator: a C++ terminal CLI that models the domain end to
end, and a thin web client backed by a Netlify serverless function that
delivers the same flows as a deployable product.

## Tech Stack

| Layer | Choice | Why |
| --- | --- | --- |
| Domain core (reference) | **C++11**, GNU Make | Demonstrates the model with no runtime deps; doubles as a teaching artifact. |
| C++ persistence | Versioned flat file, **SQLite** (optional) | Behind a `Storage` interface — zero-setup default with a real DB upgrade path. |
| C++ credential hashing | Hand-rolled **SHA-256** + 100k iteration salted KDF | Demonstrates a slow hash from first principles. |
| Web frontend | Vanilla **HTML/CSS/JS** (no framework, no bundler) | One page, three files; no toolchain to maintain. |
| Web backend | **Node.js 18+** (`node:http`, ES modules), zero dependencies | Single `server.js` serves the static frontend and the JSON API. |
| Web persistence | Local `bank-state.json` file | Trivial to inspect; perfect for a personal demo. |
| Web credential hashing | Node `crypto.scrypt` | Battle tested slow KDF, no third party deps. |
| Web sessions | HMAC-SHA256 signed tokens (hand-rolled) | Stateless auth without pulling in a JWT library. |

## Executive Summary

CyberBank CLI is a lightweight, dependency free banking engine designed to model
the core operations of a digital bank: account creation, secure login, deposits,
withdrawals, balance inquiry, inter account transfers, configurable interest
accrual, and transaction history, with durable persistence to a flat file or to
SQLite. PIN credentials are stored as salted, iterated SHA-256 hashes — the
plain text PIN never touches the disk. Built entirely on the C++ standard
library (with optional SQLite linkage), it ships as a single self contained
binary that runs anywhere a C++11 compiler exists, with zero runtime
installation cost. The architecture is intentionally small, readable, and
extensible, making it a credible foundation for fintech prototyping, classroom
instruction, and embedded retail kiosks.

## The Opportunity

Modern banking software stacks are heavy, license bound, and slow to iterate on.
Educators, hackathon teams, and early stage fintech founders need a clean,
inspectable starting point that demonstrates the essential transactional
primitives without dragging in a database server, a web framework, or a build
toolchain that takes hours to configure. CyberBank CLI fills that gap. It boots
in milliseconds, requires nothing beyond a C++ compiler, and exposes every
moving part of a deposit account ledger in a small, readable codebase.

## Product Overview

The application presents a numbered menu in the terminal. Users can:

1.  Create a new account with a name and a 4 digit PIN.
2.  Log in using an account number and PIN. Interest accrues on login.
3.  Deposit funds into the active session.
4.  Withdraw funds, with overdraft protection.
5.  Check the current balance.
6.  View the full transaction history with timestamps.
7.  Transfer funds to another account, with optional save-as-payee.
8.  Manage the payee directory (add and remove named payees).
9.  Configure the annual interest rate and accrue interest on demand.
10. Export the full transaction history to a CSV file.
11. Save state and exit cleanly.

All state mutations are persisted immediately, so a crash or power loss never
costs more than the keystroke in flight.

## Technical Architecture

The codebase is organized into a small set of cohesive modules.

* `Transaction` is a value type that records the action, amount, an optional
  note (e.g. transfer counterparty), and a human readable timestamp.
* `Account` encapsulates one customer record. Balance, hashed PIN, salt,
  payee directory, interest rate, and ledger are private; mutation flows
  through audited public methods such as `deposit`, `withdraw`,
  `recordTransferOut`, `recordTransferIn`, `accrueInterest`, and `verifyPin`.
* `Crypto` implements SHA-256 in pure C++ and exposes a salted, iterated
  PIN hashing primitive used by `Account` for credential storage.
* `Storage` is an abstract persistence interface. Two implementations ship:
  `FileStorage` (a versioned flat file) and `SqliteStorage` (SQLite via the
  C API). The `Bank` class is decoupled from the storage backend.
* `Bank` is the orchestrator. It owns the vector of accounts, generates
  sequential account numbers starting at 1001, validates user input, drives
  transfers between accounts, and delegates persistence to the active
  `Storage`.
* `main.cpp` selects the storage backend from environment variables and
  starts the menu loop.

### Credential Security

PINs are never stored in plain text. On account creation the application
generates a 16 byte random salt, then derives a 32 byte digest by hashing the
salt and PIN through 100,000 iterations of SHA-256. Only the salt and final
digest are persisted. PIN verification recomputes the same chain and performs
a string comparison against the stored hash. The iteration count makes brute
forcing a captured database expensive, and the per-account salt defeats
precomputed rainbow tables.

### Data Persistence

Two storage backends are available behind a common `Storage` interface, so the
domain classes have no I/O coupling beyond the interface contract.

* **File backend** (default). Account state is written to a versioned, pipe
  delimited flat file (`accounts.txt` by default). Each record contains the
  account number, owner name, hashed PIN, salt, balance, interest rate, last
  interest accrual timestamp, payee directory, and a semicolon delimited
  ledger of transactions. The format is human readable, trivially diffable,
  and requires no third party database.
* **SQLite backend**. Compiled in with `make SQLITE=1` and selected at
  runtime by setting `CYBERBANK_STORAGE=sqlite`. Accounts and metadata are
  stored in a `cyberbank.db` SQLite database, written transactionally on
  each save.

### Inter Account Transfers and Payee Directory

The transfer flow validates that the destination account exists and the
sender has sufficient funds, then atomically debits the sender and credits
the recipient with paired `TRANSFER_OUT` and `TRANSFER_IN` ledger entries
that record the counterparty account number. After a successful transfer
the user is offered the option to save the recipient to a named payee
directory for one click reuse.

### Interest Accrual

Each account carries its own configurable annual interest rate and a
timestamp of the last accrual. Interest is computed pro rata as
`balance * rate * (days_elapsed / 365)`, rounded down to the cent, and
credited as a labeled `INTEREST` ledger entry. Accrual runs automatically
at login when at least one full day has elapsed, and can also be triggered
on demand from the interest settings menu.

### CSV Export

The CSV export writes the active account's full transaction history to
`acct_<number>_history.csv`, with columns `timestamp,type,amount,note`.
Fields containing commas, quotes, or newlines are quoted and escaped per
RFC 4180, so the output drops cleanly into spreadsheets or analytics
pipelines.

### Input Validation

Every external input is validated before it touches business logic. PINs are
length and digit checked. Deposit, withdrawal, and transfer amounts are
parsed safely and must be positive. Withdrawals and transfers are rejected
when they exceed the available balance. Login attempts with unknown account
numbers fail closed. Payee nicknames forbid the `|`, `,`, and `:` delimiters
used by the file format.

## Concepts Demonstrated

CyberBank CLI is a complete tour of foundational C++ topics.

* Object oriented programming with classes and objects
* Public and private data with controlled accessors
* Polymorphism through the abstract `Storage` interface
* `std::vector` and `std::map` for ledger and payee collections
* `std::unique_ptr` for owned, polymorphic resources
* File handling with `ifstream` and `ofstream`
* External library integration via the SQLite C API
* Implementation of a cryptographic primitive (SHA-256) from first principles
* Conditionals and loops driving the menu and validation
* Free functions and member functions across multiple translation units
* Defensive input validation at every user boundary

## Build and Run

The project ships with a Makefile. From the project directory run:

```
make
./cyberbank
```

To build with the SQLite backend (requires `libsqlite3` and headers):

```
make SQLITE=1
CYBERBANK_STORAGE=sqlite ./cyberbank
```

To clean build artifacts:

```
make clean
```

The default build target requires only `g++` with C++11 support, which is
standard on macOS, Linux, and Windows via MinGW or WSL.

### Environment Variables

* `CYBERBANK_STORAGE` — `file` (default) or `sqlite`.
* `CYBERBANK_FILE` — path for the file backend (default `accounts.txt`).
* `CYBERBANK_DB` — path for the SQLite backend (default `cyberbank.db`).

## Web Client

A minimal browser front end lives in [`web/`](web/) and talks to a single
Node HTTP server in [`server.js`](server.js). The server mirrors the C++
domain (createAccount, login, deposit, withdraw, transfer, history) with PINs
hashed via Node's `scrypt` and state persisted to a local `bank-state.json`
file. There are no third party dependencies.

### Run locally

```
node server.js
```

Then open `http://localhost:3000`.

`PORT` and `BANK_SECRET` (used to sign session tokens) can be set via env
vars; defaults are fine for local use.

### API surface

`POST /api/bank` accepts JSON of the form `{ "action": "...", ...payload }`
and returns JSON. Authenticated actions require an
`Authorization: Bearer <token>` header obtained from `login`.

| Action | Auth | Body | Returns |
| --- | --- | --- | --- |
| `createAccount` | no | `{ name, pin }` | `{ account }` |
| `login` | no | `{ accountNumber, pin }` | `{ token, account }` |
| `me` | yes | — | `account` |
| `deposit` / `withdraw` | yes | `{ amount }` | `account` |
| `transfer` | yes | `{ to, amount }` | `account` |

## Roadmap

Medium term, the same domain core can power a REST API, a web dashboard, or a
mobile companion app, because the business rules live entirely inside
`Account` and `Bank` and have no I/O coupling beyond the `Storage` interface.
Natural next steps include scheduled recurring transfers, multi-currency
support, an admin role with audit-only ledger access, and a network
replication backend implemented as another `Storage` subclass.

## Why This Wins

CyberBank CLI is small enough to read in one sitting, complete enough to teach
the full lifecycle of a deposit account, and structured well enough to grow.
It is a credible starting point for any team that wants to prototype financial
software on a constrained timeline, and a teaching artifact that compresses the
core curriculum of object oriented C++ — including credential hashing,
polymorphic persistence, and external library integration — into a single
shippable product.
