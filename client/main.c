#include <sodium.h>
#include <assert.h>
#include <stdint.h>

#define INIT __attribute__((constructor)) void

INIT init()
{
    assert(sodium_init() == 0);
}

#define MESSAGE ((const unsigned char *) "test")
#define MESSAGE_LEN 5
#define CIPHERTEXT_LEN (crypto_secretbox_MACBYTES + MESSAGE_LEN)

unsigned char key[crypto_secretbox_KEYBYTES];
unsigned char nonce[crypto_secretbox_NONCEBYTES];
unsigned char ciphertext[CIPHERTEXT_LEN];

int main(void)
{
    printf("msg: %s, sizeof msg: %u, encrypted len: %u\n",MESSAGE,MESSAGE_LEN,CIPHERTEXT_LEN);
    crypto_secretbox_keygen(key);
    randombytes_buf(nonce, sizeof(nonce));
    crypto_secretbox_easy(ciphertext, MESSAGE, MESSAGE_LEN, nonce, key);

    unsigned char decrypted[MESSAGE_LEN];
    assert(crypto_secretbox_open_easy(decrypted, ciphertext, CIPHERTEXT_LEN, nonce, key) == 0);
    printf("decrypted: %s\n",decrypted);
}