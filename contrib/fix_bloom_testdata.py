#!/usr/bin/env python3
"""Regenerate bloom_tests.cpp test vectors for Kotia's tx format.

Kotia's CTransaction serializes an extra uint32 nTime after nVersion:
    [nVersion][nTime][vin][vout][nLockTime]
and CBlock appends a vchBlockSig vector after vtx. The historical Bitcoin
test vectors lack both, so they never parsed. This script rewrites the
hardcoded hex blobs, recomputes txids, merkle roots, rewrites within-block
prevout references and every hash the tests assert on.
"""
import hashlib
import re
import struct


def dsha256(b):
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()


def read_varint(b, pos):
    n = b[pos]
    pos += 1
    if n < 0xfd:
        return n, pos
    if n == 0xfd:
        return struct.unpack_from('<H', b, pos)[0], pos + 2
    if n == 0xfe:
        return struct.unpack_from('<I', b, pos)[0], pos + 4
    return struct.unpack_from('<Q', b, pos)[0], pos + 8


def write_varint(n):
    if n < 0xfd:
        return bytes([n])
    if n <= 0xffff:
        return b'\xfd' + struct.pack('<H', n)
    if n <= 0xffffffff:
        return b'\xfe' + struct.pack('<I', n)
    return b'\xff' + struct.pack('<Q', n)


def parse_bitcoin_tx(b, pos=0):
    """Parse a legacy Bitcoin tx into a structured dict (no nTime)."""
    start = pos
    version = struct.unpack_from('<i', b, pos)[0]
    pos += 4
    n_in, pos = read_varint(b, pos)
    txins = []
    for _ in range(n_in):
        prev_hash = b[pos:pos + 32]
        prev_n = struct.unpack_from('<I', b, pos + 32)[0]
        pos += 36
        slen, pos = read_varint(b, pos)
        script = b[pos:pos + slen]
        pos += slen
        seq = struct.unpack_from('<I', b, pos)[0]
        pos += 4
        txins.append([prev_hash, prev_n, script, seq])
    n_out, pos = read_varint(b, pos)
    txouts = []
    for _ in range(n_out):
        value = struct.unpack_from('<q', b, pos)[0]
        pos += 8
        slen, pos = read_varint(b, pos)
        script = b[pos:pos + slen]
        pos += slen
        txouts.append([value, script])
    locktime = struct.unpack_from('<I', b, pos)[0]
    pos += 4
    assert pos == len(b) or pos <= len(b), "tx parse overflow"
    return {'version': version, 'vin': txins, 'vout': txouts,
            'locktime': locktime, 'end': pos, 'start': start}


def serialize_kotia_tx(tx, ntime):
    out = struct.pack('<i', tx['version']) + struct.pack('<I', ntime)
    out += write_varint(len(tx['vin']))
    for prev_hash, prev_n, script, seq in tx['vin']:
        out += prev_hash + struct.pack('<I', prev_n) + write_varint(len(script)) + script
        out += struct.pack('<I', seq)
    out += write_varint(len(tx['vout']))
    for value, script in tx['vout']:
        out += struct.pack('<q', value) + write_varint(len(script)) + script
    out += struct.pack('<I', tx['locktime'])
    return out


def txid_of(tx_bytes):
    return dsha256(tx_bytes)  # internal (little-endian) order


def merkle_root(txids):
    hashes = list(txids)
    while len(hashes) > 1:
        if len(hashes) % 2 == 1:
            hashes.append(hashes[-1])
        hashes = [dsha256(hashes[i] + hashes[i + 1]) for i in range(0, len(hashes), 2)]
    return hashes[0]


def transform_block(b):
    header = bytearray(b[:80])
    block_time = struct.unpack_from('<I', b, 68)[0]
    pos = 80
    n_tx, pos = read_varint(b, pos)
    txs = []
    for _ in range(n_tx):
        t = parse_bitcoin_tx(b, pos)
        pos = t['end']
        txs.append(t)
    assert pos == len(b), "block hex not fully consumed"

    # old bitcoin txids (raw legacy serialization, no nTime)
    old_ids = []
    for t in txs:
        raw = (struct.pack('<i', t['version']) +
               serialize_kotia_tx(t, 0)[8:])
        old_ids.append(txid_of(raw))

    # first pass with nTime, then fixpoint-remap internal prevouts
    cur = [serialize_kotia_tx(t, block_time) for t in txs]
    new_ids = [txid_of(t) for t in cur]
    old_to_new = dict(zip(old_ids, new_ids))
    for _ in range(10):
        changed = False
        for t in txs:
            for txin in t['vin']:
                h = txin[0]
                if h in old_to_new and h != old_to_new[h]:
                    txin[0] = old_to_new[h]
                    changed = True
        cur = [serialize_kotia_tx(t, block_time) for t in txs]
        new_ids = [txid_of(t) for t in cur]
        for o, n in zip(old_ids, new_ids):
            old_to_new[o] = n
        if not changed:
            break

    header[36:68] = merkle_root(new_ids)
    # CBlock serializes an extra vchBlockSig vector after vtx (empty here)
    out = bytes(header) + write_varint(n_tx) + b''.join(cur) + b'\x00'
    return out, old_ids, new_ids, bytes(header), old_to_new


def transform_tx(b, ntime, prevout_map=None):
    t = parse_bitcoin_tx(b)
    assert t['end'] == len(b), "tx hex not fully consumed"
    if prevout_map:
        for txin in t['vin']:
            if txin[0] in prevout_map:
                txin[0] = prevout_map[txin[0]]
    raw = struct.pack('<i', t['version']) + serialize_kotia_tx(t, 0)[8:]
    nt = serialize_kotia_tx(t, ntime)
    return nt, txid_of(raw), txid_of(nt)


def disp(h):
    return h[::-1].hex()


def main():
    src = open('src/test/bloom_tests.cpp').read()

    # ---- bloom_match: two standalone txs (use a plausible block-era time) ----
    NTIME = 0x4C86041B

    tx1_old = re.search(r'bloom_match.*?ParseHex\("([0-9a-f]+)"\), SER_DISK', src, re.S).group(1)
    tx1_new, old1, new1 = transform_tx(bytes.fromhex(tx1_old), NTIME)

    m = re.search(r'unsigned char ch\[\] = \{(0x[0-9a-fA-Fx, ]+)\};', src)
    spend_old = bytes(int(x, 16) for x in m.group(1).split(',') if x.strip())[:-1]  # sizeof(ch)-1
    spend_new, oldsp, newsp = transform_tx(spend_old, NTIME, prevout_map={old1: new1})

    src = src.replace(tx1_old, tx1_new.hex())
    src = re.sub(
        r'unsigned char ch\[\] = \{0x[0-9a-fA-Fx, ]+\};\n'
        r'    vector<unsigned char> vch\(ch, ch \+ sizeof\(ch\) -1\);',
        'vector<unsigned char> vch = ParseHex("%s");' % spend_new.hex(),
        src)

    # hash reference updates for the standalone txs
    src = src.replace(disp(old1), disp(new1))          # display-order inserts
    src = src.replace(old1.hex(), new1.hex())          # byte-reversed insert
    src = src.replace(disp(oldsp), disp(newsp))        # spending tx hash
    src = src.replace('00000009' + disp(oldsp)[8:], '00000009' + disp(newsp)[8:])

    # ---- block hex blobs (only those parsed into CBlock) ----
    mapping = {}
    for m in re.finditer(r'CBlock block;\s*CDataStream stream\(ParseHex\("([0-9a-f]+)"\)', src):
        bh = m.group(1)
        new_blob, old_ids, new_ids, new_header, old_to_new = transform_block(bytes.fromhex(bh))
        src = src.replace(bh, new_blob.hex())
        for o, n in zip(old_ids, new_ids):
            mapping[disp(o)] = disp(n)
        if len(old_ids) == 1:
            # rebuild expected CMerkleBlock serialization in merkle_block_3:
            # header(80) + nTransactions(4) + vHash count(1) + txid(32) +
            # vBits count(1) + bit byte(1)
            expected = (new_header.hex() + '01000000' + '01' +
                        new_ids[0].hex() + '0101')
            src = re.sub(r'vch = ParseHex\("01[0-9a-f]+0101"\);',
                         'vch = ParseHex("%s");' % expected, src, count=1)

    for old_d, new_d in sorted(mapping.items(), key=lambda kv: -len(kv[0])):
        src = src.replace('0x' + old_d, '0x' + new_d)

    open('src/test/bloom_tests.cpp', 'w').write(src)

    print("standalone tx1 :", disp(new1))
    print("spending tx    :", disp(newsp))
    for k, v in sorted(mapping.items()):
        print("txid:", k, "->", v)


if __name__ == '__main__':
    main()
