//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

ConsensusTransSetSF::ConsensusTransSetSF (NodeCache& nodeCache)
    : m_nodeCache (nodeCache)
{
}

void ConsensusTransSetSF::gotNode (bool fromFilter, const SHAMapNode& id, uint256 const& nodeHash,
                                   Blob& nodeData, SHAMapTreeNode::TNType type)
{
    if (fromFilter)
        return;

    m_nodeCache.insert (nodeHash, nodeData);

    if ((type == SHAMapTreeNode::tnTRANSACTION_NM) && (nodeData.size () > 16))
    {
        // this is a transaction, and we didn't have it
        WriteLog (lsDEBUG, TransactionAcquire) << "Node on our acquiring TX set is TXN we may not have";

        try
        {
            Serializer s (nodeData.begin () + 4, nodeData.end ()); // skip prefix
            SerializerIterator sit (s);
            SerializedTransaction::pointer stx = boost::make_shared<SerializedTransaction> (boost::ref (sit));
            assert (stx->getTransactionID () == nodeHash);
            getApp().getJobQueue ().addJob (jtTRANSACTION, "TXS->TXN",
                                           BIND_TYPE (&NetworkOPs::submitTransaction, &getApp().getOPs (), P_1, stx, NetworkOPs::stCallback ()));
        }
        catch (...)
        {
            WriteLog (lsWARNING, TransactionAcquire) << "Fetched invalid transaction in proposed set";
        }
    }
}

bool ConsensusTransSetSF::haveNode (const SHAMapNode& id, uint256 const& nodeHash,
                                    Blob& nodeData)
{
    if (m_nodeCache.retrieve (nodeHash, nodeData))
        return true;

    // VFALCO TODO Use a dependency injection here
    Transaction::pointer txn = getApp().getMasterTransaction().fetch(nodeHash, false);

    if (txn)
    {
        // this is a transaction, and we have it
        WriteLog (lsDEBUG, TransactionAcquire) << "Node in our acquiring TX set is TXN we have";
        Serializer s;
        s.add32 (HashPrefix::transactionID);
        txn->getSTransaction ()->add (s, true);
        assert (s.getSHA512Half () == nodeHash);
        nodeData = s.peekData ();
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------

AccountStateSF::AccountStateSF (uint32 ledgerSeq)
    : mLedgerSeq (ledgerSeq)
{
}

void AccountStateSF::gotNode (bool fromFilter,
                              SHAMapNode const& id,
                              uint256 const& nodeHash,
                              Blob& nodeData,
                              SHAMapTreeNode::TNType)
{
    getApp().getNodeStore ().store (hotACCOUNT_NODE, mLedgerSeq, nodeData, nodeHash);
}

bool AccountStateSF::haveNode (SHAMapNode const& id,
                               uint256 const& nodeHash,
                               Blob& nodeData)
{
    return getApp().getOPs ().getFetchPack (nodeHash, nodeData);
}

//------------------------------------------------------------------------------

TransactionStateSF::TransactionStateSF (uint32 ledgerSeq)
    : mLedgerSeq (ledgerSeq)
{
}

void TransactionStateSF::gotNode (bool fromFilter,
                                  SHAMapNode const& id,
                                  uint256 const& nodeHash,
                                  Blob& nodeData,
                                  SHAMapTreeNode::TNType type)
{
    getApp().getNodeStore ().store (
        (type == SHAMapTreeNode::tnTRANSACTION_NM) ? hotTRANSACTION : hotTRANSACTION_NODE,
        mLedgerSeq,
        nodeData,
        nodeHash);
}

bool TransactionStateSF::haveNode (SHAMapNode const& id,
                                   uint256 const& nodeHash,
                                   Blob& nodeData)
{
    return getApp().getOPs ().getFetchPack (nodeHash, nodeData);
}
