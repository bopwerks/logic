# logic

Boolean logic validator. Reads premises and a conclusion from `stdin`. The exit code is zero when the argument is valid and non-zero otherwise. Example:

```
$ ./logic <<<'p -> q p q'; echo $?
0
$ ./logic <<<'p -> q p ~q'; echo $?
1
$ 
```

To build, run `make`. Remove the `NDEBUG` flag from `CFLAGS` to enable debug logging.
