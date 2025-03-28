#include <stdio.h>
#include <string.h>

#define FLAG "CTF{r3vers3_3ngin33ring_cha11enge}"

static int check_flag(const char *input) {
    return strcmp(input, FLAG) == 0;
}

int main() {
    char buffer[128];
    printf("Enter the flag: ");
    fgets(buffer, sizeof(buffer), stdin);

    // Remove trailing newline
    buffer[strcspn(buffer, "\n")] = 0;

    if (check_flag(buffer)) {
        printf("Correct! Well done!\n");
    } else {
        printf("Wrong flag! Try again.\n");
    }

    return 0;
}
