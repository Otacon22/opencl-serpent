typedef unsigned int    uint32_t;
__kernel void serpent_encrypt(__global unsigned char *_key, __global unsigned char *_plaintext, __global uint32_t ciphertext[4])
{
    ((unsigned char *) ciphertext)[0] = _plaintext[0];
    ((unsigned char *) ciphertext)[1] = _plaintext[1];
    ((unsigned char *) ciphertext)[2] = _plaintext[2];
    ((unsigned char *) ciphertext)[3] = _plaintext[3];
    ((unsigned char *) ciphertext)[4] = _plaintext[4];
    ((unsigned char *) ciphertext)[5] = _plaintext[5];
    ((unsigned char *) ciphertext)[6] = _plaintext[6];
    ((unsigned char *) ciphertext)[7] = _plaintext[7];
    ((unsigned char *) ciphertext)[8] = _plaintext[8];
    ((unsigned char *) ciphertext)[9] = _plaintext[9];
    ((unsigned char *) ciphertext)[10] = _plaintext[10];
    ((unsigned char *) ciphertext)[11] = _plaintext[11];
    ((unsigned char *) ciphertext)[12] = _plaintext[12];
    ((unsigned char *) ciphertext)[13] = _plaintext[13];
    ((unsigned char *) ciphertext)[14] = _plaintext[14];
    ((unsigned char *) ciphertext)[15] = _plaintext[15];


}
