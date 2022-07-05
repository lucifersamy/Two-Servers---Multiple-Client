# Two-Servers-Multiple-Client
CSE344 – System Programming – Midterm project - The main idea of this project is to simulate the aforementioned paradigm with 2 process-pooled servers: Y and Z executing on the same system as the clients. There are 3 programs to be implemented: client X, server Y and server Z. There will be a single instance of Y and of Z, and there can be an arbitrary number of client processes X, all running concurrently. Each client will submit a matrix to the server Y, and receive a response (from Y or Z) about whether it is invertible or not.


shortcomings: unfortunately there is busy waiting.
