import zlib

with open('filename.raw.zlib', 'rb') as f_in:
    compressed_data = f_in.read()

try:
    decompressed_data = zlib.decompress(compressed_data)
    print(decompressed_data.decode('utf-8'))

    # Or save to a file
    # with open('hi.raw', 'wb') as f_out:
    #     f_out.write(decompressed_data)

except zlib.error as e:
    print(f"Decompression error: {e}")
    print("May not be zlib compressed data.")
