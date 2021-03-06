#!/usr/bin/python
import unittest
import numpy   as np
import pyamgcl as amg
from scipy.sparse import csr_matrix
from scipy.sparse.linalg import bicgstab, LinearOperator

def make_problem(n=256):
    h   = 1.0 / (n - 1)
    n2  = n * n
    nnz = n2 + 4 * (n - 2) * (n - 2)

    ptr = np.zeros(n2 + 1, dtype=np.int32)
    col = np.zeros(nnz,    dtype=np.int32)
    val = np.zeros(nnz,    dtype=np.float64)
    rhs = np.ones (n2,     dtype=np.float64)

    bnd = (0, n-1)

    col_stencil = np.array([-n, -1, 0,  1,  n])
    val_stencil = np.array([-1, -1, 4, -1, -1]) / (h * h)

    idx  = 0
    head = 0

    for i in range(0, n):
        for j in range(0, n):
            if i in bnd or j in bnd:
                col[head] = idx
                val[head] = 1
                rhs[idx]  = 0
                head += 1
            else:
                col[head:head+5] = col_stencil + idx
                val[head:head+5] = val_stencil
                head += 5

            idx += 1
            ptr[idx] = head

    return ( csr_matrix( (val, col, ptr) ), rhs )

class TestPyAMGCL(unittest.TestCase):
    def test_solver(self):
        (A, rhs) = make_problem(256)

        # Setup solver
        solve = amg.make_solver(A, prm={"solver.tol" : "1e-8"})

        # Solve
        x = solve(rhs)

        # Check residual
        self.assertTrue(
                np.linalg.norm(rhs - A * x) / np.linalg.norm(rhs) < 1e-8
                )

        # Solve again, now provide system matrix explicitly
        x = solve(A, rhs)

        # Check residual
        self.assertTrue(
                np.linalg.norm(rhs - A * x) / np.linalg.norm(rhs) < 1e-8
                )

    def test_preconditioner(self):
        (A, rhs) = make_problem(256)

        # Setup preconditioner
        P = amg.make_preconditioner(A)

        # Solve
        x,info = bicgstab(A, rhs, M=P, tol = 1e-8)
        self.assertTrue(info == 0)

        # Check residual
        self.assertTrue(
                np.linalg.norm(rhs - A * x) / np.linalg.norm(rhs) < 1e-8
                )

if __name__ == "__main__":
    unittest.main()
