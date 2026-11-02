/*
 *  glue.c  —  Retro68 build only
 *  gcc calls __main() at the top of main() to run static constructors.
 *  This code resource has none, and the real one in libgcc drags in
 *  the C runtime, so provide an empty one.
 */

void __main(void)
{
}
