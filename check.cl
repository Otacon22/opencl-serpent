typedef unsigned int    uint32_t;
__kernel void serpent_encrypt(__global unsigned char *_key, __global unsigned char *_plaintext, __global unsigned char *ciphertext)
{
ciphertext[0] = _plaintext[0];
ciphertext[1] = _plaintext[1];
ciphertext[2] = _plaintext[2];
ciphertext[3] = _plaintext[3];
ciphertext[4] = _plaintext[4];
ciphertext[5] = _plaintext[5];
ciphertext[6] = _plaintext[6];
ciphertext[7] = _plaintext[7];
ciphertext[8] = _plaintext[8];
ciphertext[9] = _plaintext[9];
ciphertext[10] = _plaintext[10];
ciphertext[11] = _plaintext[11];
ciphertext[12] = _plaintext[12];
ciphertext[13] = _plaintext[13];
ciphertext[14] = _plaintext[14];
ciphertext[15] = _plaintext[15];

}
