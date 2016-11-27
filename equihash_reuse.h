#include "portable_endian.h"

// Big-endian so that lexicographic array comparison is equivalent to integer
// comparison
void EhIndexToArray(const u32 i, unsigned char* array)
{
  u32 bei = htobe32(i);
  memcpy(array, &bei, sizeof(u32));
}

// Big-endian so that lexicographic array comparison is equivalent to integer
// comparison
u32 ArrayToEhIndex(const unsigned char* array)
{
  u32 bei;
  memcpy(&bei, array, sizeof(u32));
  return be32toh(bei);
}

void ExpandArray(const unsigned char* in, size_t in_len,
                 unsigned char* out, size_t out_len,
                 size_t bit_len, size_t byte_pad)
{
  assert(bit_len >= 8);
  assert(8*sizeof(uint32_t) >= 7+bit_len);

  size_t out_width { (bit_len+7)/8 + byte_pad };
  assert(out_len == 8*out_width*in_len/bit_len);

  uint32_t bit_len_mask { ((uint32_t)1 << bit_len) - 1 };

  // The acc_bits least-significant bits of acc_value represent a bit sequence
  // in big-endian order.
  size_t acc_bits = 0;
  uint32_t acc_value = 0;

  size_t j = 0;
  for (size_t i = 0; i < in_len; i++) {
    acc_value = (acc_value << 8) | in[i];
    acc_bits += 8;

    // When we have bit_len or more bits in the accumulator, write the next
    // output element.
    if (acc_bits >= bit_len) {
      acc_bits -= bit_len;
      for (size_t x = 0; x < byte_pad; x++) {
        out[j+x] = 0;
      }
      for (size_t x = byte_pad; x < out_width; x++) {
        out[j+x] = (
                       // Big-endian
                       acc_value >> (acc_bits+(8*(out_width-x-1)))
                   ) & (
                       // Apply bit_len_mask across byte boundaries
                       (bit_len_mask >> (8*(out_width-x-1))) & 0xFF
                   );
      }
      j += out_width;
    }
  }
}

bool GetIndicesFromMinimal(const u8* minimal, u64 minimal_length,
                           u32* output, u64 output_length)
{
  if (output_length != 512)
    return false;
  if (minimal_length != 1344)
    return false;

  auto minimal_size = 21*512/8;
  size_t cBitLen = 20;
  assert(((cBitLen+1)+7)/8 <= sizeof(u32));
  size_t lenIndices { 8*sizeof(u32)*minimal_size/(cBitLen+1) };
  size_t bytePad { sizeof(u32) - ((cBitLen+1)+7)/8 };
  std::vector<unsigned char> array(lenIndices);
  ExpandArray(minimal, minimal_size,
              array.data(), lenIndices, cBitLen+1, bytePad);
  int output_i = 0;
  for (unsigned i = 0; i < lenIndices; i += sizeof(u32)) {
    output[output_i++] = ArrayToEhIndex(array.data()+i);
  }
  return true;
}


void CompressArray(const unsigned char* in, size_t in_len,
                   unsigned char* out, size_t out_len,
                   size_t bit_len, size_t byte_pad)
{
  assert(bit_len >= 8);
  assert(8*sizeof(uint32_t) >= 7+bit_len);

  size_t in_width { (bit_len+7)/8 + byte_pad };
  assert(out_len == bit_len*in_len/(8*in_width));

  uint32_t bit_len_mask { ((uint32_t)1 << bit_len) - 1 };

  // The acc_bits least-significant bits of acc_value represent a bit sequence
  // in big-endian order.
  size_t acc_bits = 0;
  uint32_t acc_value = 0;

  size_t j = 0;
  for (size_t i = 0; i < out_len; i++) {
    // When we have fewer than 8 bits left in the accumulator, read the next
    // input element.
    if (acc_bits < 8) {
      acc_value = acc_value << bit_len;
      for (size_t x = byte_pad; x < in_width; x++) {
        acc_value = acc_value | (
            (
                // Apply bit_len_mask across byte boundaries
                in[j+x] & ((bit_len_mask >> (8*(in_width-x-1))) & 0xFF)
            ) << (8*(in_width-x-1))); // Big-endian
      }
      j += in_width;
      acc_bits += bit_len;
    }

    acc_bits -= 8;
    out[i] = (acc_value >> acc_bits) & 0xFF;
  }
}


bool GetMinimalFromIndices(const u32* indices, u64 indices_length, u8* output, u64 length)
{
  if (indices_length != 512)
    return false;
  if (length != 1344)
    return false;
  size_t cBitLen = 20;
  assert(((cBitLen+1)+7)/8 <= sizeof(u32));
  size_t lenIndices { ((size_t)indices_length) * sizeof(u32) };
  size_t minLen { (cBitLen+1)*lenIndices/(8*sizeof(u32)) };
  size_t bytePad { sizeof(u32) - ((cBitLen+1)+7)/8 };
  std::vector<unsigned char> array(lenIndices);
  for (unsigned i = 0; i < indices_length; i++) {
    EhIndexToArray(indices[i], array.data()+(i*sizeof(u32)));
  }
  CompressArray(array.data(), lenIndices,
                output, minLen, cBitLen+1, bytePad);
  return true;
}
