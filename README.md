# Blockchain-Based Attendance Tracking System

A simple blockchain attendance tracking system implemented in C using:

- SHA-256 hashing
- ECDSA digital signatures
- Linked-list blockchain structure
- File persistence
- Student registry validation

The system demonstrates how blockchain concepts can be applied to secure attendance records against tampering and unauthorized modifications.

---

# Features

- Student registry loaded from `students.txt`
- Genesis block creation
- Attendance block creation
- SHA-256 hashing
- ECDSA digital signatures
- Blockchain validation
- Tamper detection
- CLI-based interface

---

# Project Structure

```
.
├── main.c
├── students.txt
└── README.md
```

---

# Compilation and execution

Required dependencies are the OpenSSL development libraries.

Compile the program using GCC:

`gcc main.c -o attendance -lssl -lcrypto`

Run the program with:

`./attendance`
