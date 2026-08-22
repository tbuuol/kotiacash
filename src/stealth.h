// Copyright (c) 2024 Kotia developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_STEALTH_H
#define BITCOIN_STEALTH_H

#include "key.h"
#include "pubkey.h"
#include "script/script.h"
#include "uint256.h"
#include "serialize.h"
#include "utilstrencodings.h"

class CTransaction;

#include <string>
#include <vector>

/**
 * Stealth Addresses implementation for Kotia
 * Based on BIP47-style reusable payment codes with ECDH
 * 
 * How it works:
 * 1. Receiver generates a stealth public key (scan key + spend key)
 * 2. Sender uses ECDH to derive a shared secret
 * 3. Sender derives a one-time public key for the payment
 * 4. Receiver scans all outputs, using their scan key to detect payments
 * 5. Receiver derives the corresponding private key to spend
 */

/** Stealth address prefix for Kotia */
static const char* STEALTH_ADDRESS_PREFIX = "sm";
static const char* STEALTH_ADDRESS_PREFIX_TESTNET = "st";

/** A stealth address consists of a scan public key and a spend public key */
class CStealthAddress
{
public:
    CPubKey scanPubKey;   // Used to detect incoming payments
    CPubKey spendPubKey;  // Used to derive one-time keys
    uint32_t nSpendOutputs; // Number of spend outputs (usually 1)
    uint32_t nScanOutputs;  // Number of scan outputs (usually 0)
    std::string label;

    CStealthAddress()
    {
        nSpendOutputs = 1;
        nScanOutputs = 0;
    }

    bool IsValid() const
    {
        return scanPubKey.IsFullyValid() && spendPubKey.IsFullyValid();
    }

    /** Encode stealth address to base58 string */
    std::string Encode(bool fTestNet = false) const;

    /** Decode base58 string to stealth address */
    bool Decode(const std::string& encoded);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(scanPubKey);
        READWRITE(spendPubKey);
        READWRITE(nSpendOutputs);
        READWRITE(nScanOutputs);
        READWRITE(LIMITED_STRING(label, 256));
    }
};

/** Stealth key pair - stored in wallet */
class CStealthKeyMetadata
{
public:
    CKeyID scanKeyID;    // ID of the scan private key
    CKeyID spendKeyID;   // ID of the spend private key
    uint32_t nScanOutputs;
    uint32_t nSpendOutputs;

    CStealthKeyMetadata()
    {
        nScanOutputs = 0;
        nSpendOutputs = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(scanKeyID);
        READWRITE(spendKeyID);
        READWRITE(nScanOutputs);
        READWRITE(nSpendOutputs);
    }
};

/** One-time key pair derived for a specific stealth payment */
class CStealthSharedSecret
{
public:
    CKey ephemeralKey;    // Ephemeral private key (sender) or derived private key (receiver)
    CPubKey ephemeralPubKey; // Ephemeral public key included in transaction

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (ser_action.ForRead()) {
            std::vector<unsigned char> vch;
            READWRITE(vch);
            if (!vch.empty()) {
                ephemeralKey.Set(vch.begin(), vch.end(), true);
                ephemeralPubKey = ephemeralKey.GetPubKey();
            }
        } else {
            std::vector<unsigned char> vch;
            if (ephemeralKey.IsValid()) {
                vch.assign(ephemeralKey.begin(), ephemeralKey.end());
            }
            READWRITE(vch);
        }
    }
};

/**
 * Generate a new stealth key pair
 * @param scanKey Output: scan private key
 * @param spendKey Output: spend private key
 * @return true on success
 */
bool GenerateStealthKeyPair(CKey& scanKey, CKey& spendKey);

/**
 * Create a stealth address from public keys
 * @param scanPubKey Scan public key
 * @param spendPubKey Spend public key
 * @return CStealthAddress
 */
CStealthAddress CreateStealthAddress(const CPubKey& scanPubKey, const CPubKey& spendPubKey);

/**
 * Derive a one-time public key for sending to a stealth address
 * @param stealthAddr Recipient's stealth address
 * @param ephemKey Output: ephemeral private key (for signing)
 * @return One-time public key to send payment to
 */
CPubKey DeriveOneTimePublicKey(const CStealthAddress& stealthAddr, CKey& ephemKey);

/**
 * Scan a public key to check if it belongs to us (receiver side)
 * @param scanPrivKey Our scan private key
 * @param spendPrivKey Our spend private key
 * @param outputPubKey The public key from a transaction output
 * @param ephemPubKey The ephemeral public key (from OP_RETURN or prefix)
 * @param oneTimePrivKey Output: derived one-time private key if match found
 * @return true if the output belongs to us
 */
bool ScanStealthOutput(const CKey& scanPrivKey, const CKey& spendPrivKey,
                       const CPubKey& outputPubKey, const CPubKey& ephemPubKey,
                       CKey& oneTimePrivKey);

/**
 * Create a stealth output script (P2PKH to one-time key)
 * @param oneTimePubKey The one-time public key
 * @return Script for the payment output
 */
CScript CreateStealthOutputScript(const CPubKey& oneTimePubKey);

/**
 * Extract ephemeral public key from transaction
 * Looks for OP_RETURN with ephemeral key data
 * @param tx Transaction to scan
 * @param ephemPubKey Output: extracted ephemeral public key
 * @return true if ephemeral key found
 */
bool ExtractEphemeralKey(const CTransaction& tx, CPubKey& ephemPubKey);

/**
 * Check if a transaction output is a stealth payment
 * @param tx Transaction
 * @param nOut Output index
 * @param scanPrivKey Our scan key
 * @param spendPrivKey Our spend key
 * @param oneTimePrivKey Output: derived private key if ours
 * @return true if this is a stealth payment to us
 */
bool IsStealthPayment(const CTransaction& tx, unsigned int nOut,
                      const CKey& scanPrivKey, const CKey& spendPrivKey,
                      CKey& oneTimePrivKey);

/**
 * Generate a random nonce for stealth address encoding
 * @return 8-byte nonce
 */
std::vector<unsigned char> GenerateStealthNonce();

#endif // BITCOIN_STEALTH_H
