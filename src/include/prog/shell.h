#ifndef PROG_SHELL_H
#define PROG_SHELL_H

/**
 * fs_test_shell - Start an interactive filesystem test shell
 *
 * Input:  None (uses keyboard for input, VGA for output)
 * Output: Returns when the user types "exit".
 *
 * Supported commands: ls, cat, touch, write, mkdir, rm, rmdir, stat, help, exit
 */
void fs_test_shell(void);

/**
 * test_filesystem - Run automated filesystem smoke tests
 *
 * Input:  None (requires a mounted root ext2 filesystem)
 * Output: Prints PASS/FAIL for each test to the terminal.
 */
void test_filesystem(void);

#endif /* PROG_SHELL_H */
