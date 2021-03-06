/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

// A nym contains a list of credentials
//
// Each credential contains a "master" subkey, and a list of subkeys
// signed by that master.
//
// The same class (subkey) is used because there are master credentials
// and subkey credentials, so we're using a single "subkey" class to
// encapsulate each credential, both for the master credential and
// for each subkey credential.
//
// Each subkey has 3 key pairs: encryption, signing, and authentication.
//
// Each key pair has 2 OTAsymmetricKeys (public and private.)

#include <opentxs/core/stdafx.hpp>

#include <opentxs/core/crypto/OTKeyCredential.hpp>

#include <opentxs/core/crypto/OTCredential.hpp>
#include <opentxs/core/Log.hpp>
#include <opentxs/core/crypto/OTSignature.hpp>
#include <opentxs/core/OTStorage.hpp>

namespace opentxs
{

bool OTKeyCredential::VerifySignedBySelf()
{
    return VerifyWithKey(m_SigningKey.GetPublicKey());
}

// NOTE: You might ask, if we are using theSignature's metadata to narrow down
// the key type,
// then why are we still passing the key type as a separate parameter? Good
// question. Because
// often, theSignature will have no metadata at all! In that case, normally we
// would just NOT
// return any keys, period. Because we assume, if a key credential signed it,
// then it WILL have
// metadata, and if it doesn't have metadata, then a key credential did NOT sign
// it, and therefore
// we know from the get-go that none of the keys from the key credentials will
// work to verify it,
// either. That's why, normally, we don't return any keys if theSignature has no
// metadata.
// BUT...Let's say you know this, that the signature has no metadata, yet you
// also still believe
// it may be signed with one of these keys. Further, while you don't know
// exactly which key it
// actually is, let's say you DO know by context that it's a signing key, or an
// authentication key,
// or an encryption key. So you specify that. In which case, OT should return
// all possible matching
// pubkeys based on that 1-letter criteria, instead of its normal behavior,
// which is to return all
// possible matching pubkeys based on a full match of the metadata.
//
int32_t OTKeyCredential::GetPublicKeysBySignature(
    listOfAsymmetricKeys& listOutput, const OTSignature& theSignature,
    char cKeyType) const // 'S' (signing key) or 'E' (encryption key)
                         // or 'A' (authentication key)
{
    // Key type was not specified, because we only want keys that match the
    // metadata on theSignature.
    // And if theSignature has no metadata, then we want to return 0 keys.
    if (('0' == cKeyType) && !theSignature.getMetaData().HasMetadata())
        return 0;

    // By this point, we know that EITHER exact metadata matches must occur, and
    // the signature DOES have metadata, ('0')
    // OR the search is only for 'A', 'E', or 'S' candidates, based on cKeyType,
    // and that the signature's metadata
    // can additionally narrow the search down, if it's present, which in this
    // case it's not guaranteed to be.
    int32_t nCount = 0;
    switch (cKeyType) {
    // Specific search only for signatures with metadata.
    // FYI, theSignature.getMetaData().HasMetadata() is true, in this case.
    case '0': {
        // That's why I can just assume theSignature has a key type here:
        switch (theSignature.getMetaData().GetKeyType()) {
        case 'A':
            nCount =
                m_AuthentKey.GetPublicKeyBySignature(listOutput, theSignature);
            break; // bInclusive=false by default
        case 'E':
            nCount =
                m_EncryptKey.GetPublicKeyBySignature(listOutput, theSignature);
            break; // bInclusive=false by default
        case 'S':
            nCount =
                m_SigningKey.GetPublicKeyBySignature(listOutput, theSignature);
            break; // bInclusive=false by default
        default:
            otErr << __FUNCTION__
                  << ": Unexpected keytype value in signature metadata: "
                  << theSignature.getMetaData().GetKeyType() << " (failure)\n";
            return 0;
        }
        break;
    }
    // Generalized search which specifies key type and returns keys
    // even for signatures with no metadata. (When metadata is present,
    // it's still used to eliminate keys.)
    case 'A':
        nCount = m_AuthentKey.GetPublicKeyBySignature(listOutput, theSignature,
                                                      true);
        break; // bInclusive=true
    case 'E':
        nCount = m_EncryptKey.GetPublicKeyBySignature(listOutput, theSignature,
                                                      true);
        break; // bInclusive=true
    case 'S':
        nCount = m_SigningKey.GetPublicKeyBySignature(listOutput, theSignature,
                                                      true);
        break; // bInclusive=true
    default:
        otErr << __FUNCTION__
              << ": Unexpected value for cKeyType (should be 0, A, E, or S): "
              << cKeyType << "\n";
        return 0;
    }
    return nCount;
}

// Verify that m_strNymID is the same as the hash of m_strSourceForNymID.
// Subkey verifies (master does not): NymID against Masterkey NymID, and master
// credential ID against hash of master credential.
// (How? Because OTMasterkey overrides this function and does NOT call the
// parent, whereas OTSubkey does.)
// Then verify the (self-signed) signature on *this.
//
bool OTKeyCredential::VerifyInternally()
{
    // Verify that m_strNymID is the same as the hash of m_strSourceForNymID.
    if (!ot_super::VerifyInternally()) return false;

    // Any OTKeyCredential (both master and subkeys, but no other credentials)
    // must ** sign itself.**
    //
    if (!VerifySignedBySelf()) {
        otOut << __FUNCTION__ << ": Failed verifying key credential: it's not "
                                 "signed by itself (its own signing key.)\n";
        return false;
    }

    return true;
}

// otErr << "%s line %d: \n", __FILE__, __LINE__);

OTKeyCredential::OTKeyCredential()
    : ot_super()
{
}
OTKeyCredential::OTKeyCredential(OTCredential& theOwner)
    : ot_super(theOwner)
{
}

OTKeyCredential::~OTKeyCredential()
{
    Release_Subkey();
}

// virtual
void OTKeyCredential::Release()
{
    Release_Subkey(); // My own cleanup is done here.

    // Next give the base class a chance to do the same...
    ot_super::Release(); // since I've overridden the base class, I call it
                         // now...
}

void OTKeyCredential::Release_Subkey()
{
    // Release any dynamically allocated members here. (Normally.)
}

bool OTKeyCredential::GenerateKeys(int32_t nBits) // Gotta start
                                                  // somewhere.
{
    const bool bSign = m_SigningKey.MakeNewKeypair(nBits);
    const bool bAuth = m_AuthentKey.MakeNewKeypair(nBits);
    const bool bEncr = m_EncryptKey.MakeNewKeypair(nBits);

    OT_ASSERT(bSign && bAuth && bEncr);

    m_SigningKey.SaveAndReloadBothKeysFromTempFile(); // Keys won't be right
                                                      // until this happens.
    m_AuthentKey.SaveAndReloadBothKeysFromTempFile(); // (Necessary evil until
                                                      // better fix.)
    m_EncryptKey.SaveAndReloadBothKeysFromTempFile();

    // Since the keys were all generated successfully, we need to copy their
    // certificate data into the m_mapPublicInfo and m_mapPrivateInfo (string
    // maps.)
    //
    String strPublicKey, strPrivateCert;
    String::Map mapPublic, mapPrivate;

    const String strReason("Generating keys for new credential...");

    const bool b1 = m_SigningKey.GetPublicKey(
        strPublicKey, false); // bEscaped=true by default.
    const bool b2 =
        m_SigningKey.SaveCertAndPrivateKeyToString(strPrivateCert, &strReason);

    if (b1)
        mapPublic.insert(
            std::pair<std::string, std::string>("S", strPublicKey.Get()));
    if (b2)
        mapPrivate.insert(
            std::pair<std::string, std::string>("S", strPrivateCert.Get()));

    strPublicKey.Release();
    strPrivateCert.Release();
    const bool b3 = m_AuthentKey.GetPublicKey(
        strPublicKey, false); // bEscaped=true by default.
    const bool b4 =
        m_AuthentKey.SaveCertAndPrivateKeyToString(strPrivateCert, &strReason);

    if (b3)
        mapPublic.insert(
            std::pair<std::string, std::string>("A", strPublicKey.Get()));
    if (b4)
        mapPrivate.insert(
            std::pair<std::string, std::string>("A", strPrivateCert.Get()));

    strPublicKey.Release();
    strPrivateCert.Release();
    const bool b5 = m_EncryptKey.GetPublicKey(
        strPublicKey, false); // bEscaped=true by default.
    const bool b6 =
        m_EncryptKey.SaveCertAndPrivateKeyToString(strPrivateCert, &strReason);

    if (b5)
        mapPublic.insert(
            std::pair<std::string, std::string>("E", strPublicKey.Get()));
    if (b6)
        mapPrivate.insert(
            std::pair<std::string, std::string>("E", strPrivateCert.Get()));

    if (3 != mapPublic.size()) {
        otErr << "In " << __FILE__ << ", line " << __LINE__
              << ": Failed getting public keys in "
                 "OTKeyCredential::GenerateKeys.\n";
        return false;
    }
    else
        ot_super::SetPublicContents(mapPublic);

    if (3 != mapPrivate.size()) {
        otErr << "In " << __FILE__ << ", line " << __LINE__
              << ": Failed getting private keys in "
                 "OTKeyCredential::GenerateKeys.\n";
        return false;
    }
    else
        ot_super::SetPrivateContents(mapPrivate);

    return true;
}

// virtual
bool OTKeyCredential::SetPublicContents(const String::Map& mapPublic)
{
    // NOTE: We might use this on the server side, we'll see, but so far on the
    // client
    // side, we won't need to use this function, since SetPrivateContents
    // already does
    // the dirty work of extracting the public keys and setting them.
    //

    if (mapPublic.size() != 3) {
        otErr << __FILE__ << " line " << __LINE__
              << ": Failure: Expected 3 in mapPublic.size(), but the actual "
                 "value was: " << mapPublic.size() << "\n";
        return false;
    }

    auto itAuth = mapPublic.find("A"); // Authentication key
    if (mapPublic.end() == itAuth) {
        otErr << __FILE__ << " line " << __LINE__
              << ": Failure: Unable to find public authentication key.\n";
        return false;
    }

    auto itEncr = mapPublic.find("E"); // Encryption key
    if (mapPublic.end() == itEncr) {
        otErr << __FILE__ << " line " << __LINE__
              << ": Failure: Unable to find public encryption key.\n";
        return false;
    }

    auto itSign = mapPublic.find("S"); // Signing key
    if (mapPublic.end() == itSign) {
        otErr << __FILE__ << " line " << __LINE__
              << ": Failure: Unable to find public signing key.\n";
        return false;
    }

    if (ot_super::SetPublicContents(mapPublic)) {

        String strKey;
        strKey.Set(itAuth->second.c_str());
        if (!m_AuthentKey.SetPublicKey(strKey)) {
            otErr << __FILE__ << " line " << __LINE__
                  << ": Failure: Unable to set public authentication key based "
                     "on string:\n" << strKey << "\n";
            return false;
        }

        strKey.Release();
        strKey.Set(itEncr->second.c_str());
        if (!m_EncryptKey.SetPublicKey(strKey)) {
            otErr << __FILE__ << " line " << __LINE__
                  << ": Failure: Unable to set public encryption key based on "
                     "string:\n" << strKey << "\n";
            return false;
        }

        strKey.Release();
        strKey.Set(itSign->second.c_str());
        if (!m_SigningKey.SetPublicKey(strKey)) {
            otErr << __FILE__ << " line " << __LINE__
                  << ": Failure: Unable to set public signing key based on "
                     "string:\n" << strKey << "\n";
            return false;
        }

        return true; // SUCCESS! This means the input, mapPublic, actually
                     // contained an "A" key, an "E"
        // key, and an "S" key (and nothing else) and that all three of those
        // public keys actually loaded
        // from string form into their respective key object members.
    }

    return false;
}

// NOTE: With OTKeyCredential, if you call SetPrivateContents, you don't have to
// call SetPublicContents,
// since SetPrivateContents will automatically set the public contents, since
// the public certs are available
// from the private certs. Not all credentials do this, but keys do. So you
// might ask, why did I still
// provide a version of SetPublicContents for OTKeyCredential? Just to fit the
// convention, also also because
// perhaps on the server side, public contents will be the only ones available,
// and private ones will never
// be set.

// virtual
bool OTKeyCredential::SetPrivateContents(
    const String::Map& mapPrivate,
    const OTPassword* pImportPassword) // if not nullptr, it means to use this
                                       // password by default.
{

    if (mapPrivate.size() != 3) {
        otErr << __FILE__ << " line " << __LINE__
              << ": Failure: Expected 3 in mapPrivate(), but the actual value "
                 "was: " << mapPrivate.size() << "\n";
        return false;
    }

    auto itAuth = mapPrivate.find("A"); // Authentication key
    if (mapPrivate.end() == itAuth) {
        otErr << __FILE__ << " line " << __LINE__
              << ": Failure: Unable to find private authentication key.\n";
        return false;
    }

    auto itEncr = mapPrivate.find("E"); // Encryption key
    if (mapPrivate.end() == itEncr) {
        otErr << __FILE__ << " line " << __LINE__
              << ": Failure: Unable to find private encryption key.\n";
        return false;
    }

    auto itSign = mapPrivate.find("S"); // Signing key
    if (mapPrivate.end() == itSign) {
        otErr << __FILE__ << " line " << __LINE__
              << ": Failure: Unable to find private signing key.\n";
        return false;
    }

    if (ot_super::SetPrivateContents(mapPrivate, pImportPassword)) {
        const String strReason("Loading private key from credential.");
        String::Map mapPublic;

        String strPrivate;
        strPrivate.Set(itAuth->second.c_str()); // strPrivate now contains the
                                                // private Cert string.

        if (false ==
            m_AuthentKey.LoadPrivateKeyFromCertString(
                strPrivate, false /*bEscaped true by default*/, &strReason,
                pImportPassword)) {
            otErr << __FILE__ << " line " << __LINE__
                  << ": Failure: Unable to set private authentication key "
                     "based on string.\n";
            //          otErr << __FILE__ << " line " << __LINE__ <<
            //            ": Failure: Unable to set private authentication key
            // based on string:\n" << strPrivate << "\n";
            return false;
        }
        else // Success loading the private key. Let's grab the public key
               // here.
        {
            String strPublic;

            if ((false ==
                 m_AuthentKey.LoadPublicKeyFromCertString(
                     strPrivate, false /* bEscaped true by default */,
                     &strReason, pImportPassword)) ||
                (false ==
                 m_AuthentKey.GetPublicKey(
                     strPublic, false /* bEscaped true by default */))) {
                otErr << __FILE__ << " line " << __LINE__
                      << ": Failure: Unable to set public authentication key "
                         "based on private string.\n";
                //              otErr << __FILE__ << " line " << __LINE__ <<
                //                ": Failure: Unable to set public
                // authentication key based on private string:\n" << strPrivate
                // << "\n";
                return false;
            }
            mapPublic.insert(
                std::pair<std::string, std::string>("A", strPublic.Get()));
        }

        strPrivate.Release();
        strPrivate.Set(itEncr->second.c_str());

        if (false ==
            m_EncryptKey.LoadPrivateKeyFromCertString(
                strPrivate, false /*bEscaped true by default*/, &strReason,
                pImportPassword)) {
            otErr << __FILE__ << " line " << __LINE__
                  << ": Failure: Unable to set private encryption key based on "
                     "string.\n";
            //          otErr << __FILE__ << " line " << __LINE__ <<
            //            ": Failure: Unable to set private encryption key based
            // on string:\n" << strPrivate << "\n";
            return false;
        }
        else // Success loading the private key. Let's grab the public key
               // here.
        {
            String strPublic;

            if ((false ==
                 m_EncryptKey.LoadPublicKeyFromCertString(
                     strPrivate, false /* bEscaped true by default */,
                     &strReason, pImportPassword)) ||
                (false ==
                 m_EncryptKey.GetPublicKey(
                     strPublic, false /* bEscaped true by default */))) {
                otErr << __FILE__ << " line " << __LINE__
                      << ": Failure: Unable to set public encryption key based "
                         "on private string.\n";
                //              otErr << __FILE__ << " line " << __LINE__ <<
                //                ": Failure: Unable to set public encryption
                // key based on private string:\n" << strPrivate << "\n";
                return false;
            }
            mapPublic.insert(
                std::pair<std::string, std::string>("E", strPublic.Get()));
        }

        strPrivate.Release();
        strPrivate.Set(itSign->second.c_str());

        if (false ==
            m_SigningKey.LoadPrivateKeyFromCertString(
                strPrivate, false /*bEscaped true by default*/, &strReason,
                pImportPassword)) {
            otErr << __FILE__ << " line " << __LINE__
                  << ": Failure: Unable to set private signing key based on "
                     "string.\n";
            //          otErr << __FILE__ << " line " << __LINE__ <<
            //            ": Failure: Unable to set private signing key based on
            // string:\n" << strPrivate << "\n";
            return false;
        }
        else // Success loading the private key. Let's grab the public key
               // here.
        {
            String strPublic;

            if ((false ==
                 m_SigningKey.LoadPublicKeyFromCertString(
                     strPrivate, false /* bEscaped true by default */,
                     &strReason, pImportPassword)) ||
                (false ==
                 m_SigningKey.GetPublicKey(
                     strPublic, false /* bEscaped true by default */))) {
                otErr << __FILE__ << " line " << __LINE__
                      << ": Failure: Unable to set public signing key based on "
                         "private string.\n";
                //              otErr << __FILE__ << " line " << __LINE__ <<
                //                ": Failure: Unable to set public signing key
                // based on private string:\n" << strPrivate << "\n";
                return false;
            }
            mapPublic.insert(
                std::pair<std::string, std::string>("S", strPublic.Get()));
        }

        if (!ot_super::SetPublicContents(mapPublic)) {
            otErr << __FILE__ << " line " << __LINE__
                  << ": Failure: While trying to call: "
                     "ot_super::SetPublicContents(mapPublic)\n"; // Should never
                                                                 // happen (it
                                                                 // always just
                                                                 // returns
                                                                 // true.)
            return false;
        }

        return true; // SUCCESS! This means the input, mapPrivate, actually
                     // contained an "A" key, an "E"
        // key, and an "S" key (and nothing else) and that all three of those
        // private keys actually loaded
        // from string form into their respective key object members. We also
        // set the public keys in here, FYI.
    }

    return false;
}

bool OTKeyCredential::Sign(Contract& theContract, const OTPasswordData* pPWData)
{
    return m_SigningKey.SignContract(theContract, pPWData);
}

bool OTKeyCredential::ReEncryptKeys(const OTPassword& theExportPassword,
                                    bool bImporting)
{
    String strSign, strAuth, strEncr;

    const bool bSign =
        m_SigningKey.ReEncrypt(theExportPassword, bImporting, strSign);
    bool bAuth = false;
    bool bEncr = false;

    if (bSign) {
        bAuth = m_AuthentKey.ReEncrypt(theExportPassword, bImporting, strAuth);

        if (bAuth)
            bEncr =
                m_EncryptKey.ReEncrypt(theExportPassword, bImporting, strEncr);
    }

    const bool bSuccessReEncrypting = (bSign && bAuth && bEncr);
    bool bSuccess = false;

    // If success, we now have the updated versions of the private certs.
    //
    if (bSuccessReEncrypting) {
        String::Map mapPrivate;

        for (auto& it : m_mapPrivateInfo) {
            std::string str_key_type = it.first; // A, E, S.
            std::string str_key_contents = it.second;

            if ("A" == str_key_type) {
                mapPrivate.insert(
                    std::pair<std::string, std::string>("A", strAuth.Get()));
            }
            else if ("E" == str_key_type)
                mapPrivate.insert(
                    std::pair<std::string, std::string>("E", strEncr.Get()));
            else if ("S" == str_key_type)
                mapPrivate.insert(
                    std::pair<std::string, std::string>("S", strSign.Get()));

            else // Should never happen, but if there are other keys here, we'll
                 // preserve 'em.
            {
                mapPrivate.insert(std::pair<std::string, std::string>(
                    str_key_type, str_key_contents));
                OT_FAIL; // really this should never happen.
            }
        }

        if (3 != mapPrivate.size())
            otErr << __FUNCTION__ << ": Unexpected, mapPrivate does not have "
                                     "exactly a size of 3. \n";

        else {
            // Logic: If I'm IMPORTING, bImporting is true, and that means the
            // Nym WAS
            // encrypted to its own external passphrase (whenever it was
            // exported) and
            // in order to IMPORT it, I re-encrypted all the keys from the
            // exported passphrase,
            // to the wallet master key (this was done above in ReEncrypt.)
            // So now that I am loading up the private contents again based on
            // those
            // re-encrypted keys, I will want to use the normal wallet master
            // key to load
            // them. (So I pass nullptr.)
            //
            // But if I'm EXPORTING, bImporting is false, and that means the Nym
            // WAS
            // encrypted to the wallet's master passphrase, until just above
            // when I ReEncrypted
            // it to its own external passphrase. So now that I am attempting to
            // reload the
            // keys based on this new external passphrase, I need to pass it in
            // so it can be
            // used for that loading. (So I pass &theExportPassword.)
            //
            bSuccess = SetPrivateContents(
                mapPrivate, bImporting ? nullptr : &theExportPassword);
        }
    }

    return bSuccess; // Note: Caller must re-sign credential after doing this,
                     // to keep these changes.
}

void OTKeyCredential::SetMetadata()
{
    char cMetaKeyType;     // Can be A, E, or S (authentication, encryption, or
                           // signing. Also, E would be unusual.)
    char cMetaNymID = '0'; // Can be any letter from base58 alphabet. Represents
                           // fourth letter of a Nym's ID.
    char cMetaMasterCredID = '0'; // Can be any letter from base58 alphabet.
                                  // Represents fourth letter of a Master
                                  // Credential ID (for that Nym.)
    char cMetaSubCredID = '0';    // Can be any letter from base58 alphabet.
                                  // Represents fourth letter of a SubCredential
                                  // ID (signed by that Master.)

    String strSubcredID;
    GetIdentifier(strSubcredID);

    // If the ID is otxBk69JB8oaGLo8U6UrC4ZxXoYcRBzUSc92
    // then the character used for metadata would be 'B',
    // the fourth one. (At index 3.)
    //
    const uint32_t uIndex = 3;

    const bool bNymID = GetNymID().At(uIndex, cMetaNymID);
    const bool bCredID =
        m_pOwner->GetMasterCredID().At(uIndex, cMetaMasterCredID);
    const bool bSubID = strSubcredID.At(uIndex, cMetaSubCredID);
    // In the case of the master
    // credential, this will repeat the
    // previous one.

    if (!bNymID || !bCredID || !bSubID) {
        otWarn << __FUNCTION__ << ": No metadata available:\n "
               << "bNymID"
               << " is " << (bNymID ? "True" : "False") << ", "
               << "bCredID"
               << " is " << (bNymID ? "True" : "False") << ", "
               << "bSubID"
               << " is " << (bNymID ? "True" : "False") << "";
    }

    OTSignatureMetadata theMetadata;

    cMetaKeyType = 'A';
    theMetadata.SetMetadata(cMetaKeyType, cMetaNymID, cMetaMasterCredID,
                            cMetaSubCredID);
    m_AuthentKey.SetMetadata(theMetadata);

    cMetaKeyType = 'E';
    theMetadata.SetMetadata(cMetaKeyType, cMetaNymID, cMetaMasterCredID,
                            cMetaSubCredID);
    m_EncryptKey.SetMetadata(theMetadata);

    cMetaKeyType = 'S';
    theMetadata.SetMetadata(cMetaKeyType, cMetaNymID, cMetaMasterCredID,
                            cMetaSubCredID);
    m_SigningKey.SetMetadata(theMetadata);
}

} // namespace opentxs
