module test_c3;

extern fn void printk(char* fmt, ...);

fn void c3_test() @export @cname("c3_test") {
    printk("hello from C3!\n");
}