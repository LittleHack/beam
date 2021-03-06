// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "wallet/wallet_db.h"
#include "wallet/negotiator.h"
#include <deque>
#include "core/proto.h"

namespace beam
{
	struct IWalletObserver : IKeyChainObserver
	{
		virtual void onSyncProgress(int done, int total) = 0;
	};

    struct IWallet
    {
        using Ptr = std::shared_ptr<IWallet>;
        virtual ~IWallet() {}
        // wallet to wallet responses
        virtual void handle_tx_message(const WalletID&, wallet::Invite&&) = 0;
        virtual void handle_tx_message(const WalletID&, wallet::ConfirmTransaction&&) = 0;
        virtual void handle_tx_message(const WalletID&, wallet::ConfirmInvitation&&) = 0;
        virtual void handle_tx_message(const WalletID&, wallet::TxRegistered&&) = 0;
        virtual void handle_tx_message(const WalletID&, wallet::TxFailed&&) = 0;
        // node to wallet responses
        virtual bool handle_node_message(proto::Boolean&&) = 0;
        virtual bool handle_node_message(proto::ProofUtxo&&) = 0;
		virtual bool handle_node_message(proto::ProofState&& msg) = 0;
        virtual bool handle_node_message(proto::ProofKernel&& msg) = 0;
		virtual bool handle_node_message(proto::NewTip&&) = 0;
        virtual bool handle_node_message(proto::Mined&& msg) = 0;

        virtual void abort_sync() = 0;

		virtual void subscribe(IWalletObserver* observer) = 0;
		virtual void unsubscribe(IWalletObserver* observer) = 0;

        virtual void cancel_tx(const TxID& id) = 0;
        virtual void delete_tx(const TxID& id) = 0;

		virtual void set_node_address(io::Address node_address) = 0;
		virtual void emergencyReset() = 0;

		virtual bool get_IdentityKeyForNode(ECC::Scalar::Native&, const PeerID& idNode) = 0;
    };

    struct INetworkIO 
    {
        using Ptr = std::shared_ptr<INetworkIO>;
        virtual ~INetworkIO() {}
        virtual void set_wallet(IWallet*) = 0;
        // wallet to wallet requests
        virtual void send_tx_message(const WalletID& to, wallet::Invite&&) = 0;
        virtual void send_tx_message(const WalletID& to, wallet::ConfirmTransaction&&) = 0;
        virtual void send_tx_message(const WalletID& to, wallet::ConfirmInvitation&&) = 0;
        virtual void send_tx_message(const WalletID& to, wallet::TxRegistered&&) = 0 ;
        virtual void send_tx_message(const WalletID& to, wallet::TxFailed&&) = 0;
        // wallet to node requests
        virtual void send_node_message(proto::NewTransaction&&) = 0;
        virtual void send_node_message(proto::GetProofUtxo&&) = 0;
		virtual void send_node_message(proto::GetHdr&&) = 0;
        virtual void send_node_message(proto::GetMined&&) = 0;
        virtual void send_node_message(proto::GetProofState&&) = 0;
        virtual void send_node_message(proto::GetProofKernel&&) = 0;
        // connection control
        //virtual void close_connection(const WalletID& id) = 0;
        virtual void connect_node() = 0;
        virtual void close_node_connection() = 0;

        virtual void new_own_address(const WalletID& address) = 0;
        virtual void address_deleted(const WalletID& address) = 0;
        
		virtual void set_node_address(io::Address node_address) = 0;
    };

    class NetworkIOBase : public INetworkIO
    {
    protected:
        NetworkIOBase() : m_wallet{ nullptr }
        {

        }
        IWallet& get_wallet() const
        {
            assert(m_wallet);
            return *m_wallet;
        }
    private:
        void set_wallet(IWallet* wallet) override
        {
            m_wallet = wallet;
            if (wallet != nullptr)
            {
                connect_node();
            }
        }
        IWallet* m_wallet; // wallet holds reference to INetworkIO
    };

    class Wallet : public IWallet
                 , public wallet::INegotiatorGateway
    {
        using Callback = std::function<void()>;
    public:
        using TxCompletedAction = std::function<void(const TxID& tx_id)>;

        Wallet(IKeyChain::Ptr keyChain, INetworkIO::Ptr network, bool holdNodeConnection = false, TxCompletedAction&& action = TxCompletedAction());
        virtual ~Wallet();

        TxID transfer_money(const WalletID& from, const WalletID& to, Amount amount, Amount fee = 0, bool sender = true, ByteBuffer&& message = {} );
        void resume_tx(const TxDescription& tx);
        void resume_all_tx();

        void send_tx_invitation(const TxDescription& tx, wallet::Invite&&) override;
        void send_tx_confirmation(const TxDescription& tx, wallet::ConfirmTransaction&&) override;
        void on_tx_completed(const TxDescription& tx) override;
        void send_tx_failed(const TxDescription& tx) override;

        void send_tx_confirmation(const TxDescription& tx, wallet::ConfirmInvitation&&) override;
        void register_tx(const TxDescription& tx, Transaction::Ptr) override;
        void send_tx_registered(const TxDescription& tx) override;
        void confirm_outputs(const TxDescription&) override;

        void handle_tx_message(const WalletID&, wallet::Invite&&) override;
        void handle_tx_message(const WalletID&, wallet::ConfirmTransaction&&) override;
        void handle_tx_message(const WalletID&, wallet::ConfirmInvitation&&) override;
        void handle_tx_message(const WalletID&, wallet::TxRegistered&&) override;
        void handle_tx_message(const WalletID&, wallet::TxFailed&&) override;

        bool handle_node_message(proto::Boolean&& res) override;
        bool handle_node_message(proto::ProofUtxo&& proof) override;
		bool handle_node_message(proto::ProofState&& msg) override;
        bool handle_node_message(proto::ProofKernel&& msg) override;
		bool handle_node_message(proto::NewTip&& msg) override;
        bool handle_node_message(proto::Mined&& msg) override;

        void abort_sync() override;

		void subscribe(IWalletObserver* observer) override;
		void unsubscribe(IWalletObserver* observer) override;

        void handle_tx_registered(const TxID& txId, bool res);

        void cancel_tx(const TxID& txId) override;
        void delete_tx(const TxID& txId) override;

		void set_node_address(io::Address node_address) override;
		void emergencyReset() override;
		bool get_IdentityKeyForNode(ECC::Scalar::Native&, const PeerID& idNode) override;

    private:
        void remove_peer(const TxID& txId);
        void getUtxoProofs(const std::vector<Coin>& coins);
        void do_fast_forward();
        void get_kernel_proof(wallet::Negotiator::Ptr n);
        void get_kernel_utxo_proofs(wallet::Negotiator::Ptr n);
        void enter_sync();
        bool exit_sync();
        void report_sync_progress();
        bool close_node_connection();
        void register_tx(const TxID& txId, Transaction::Ptr);
        void resume_negotiator(const TxDescription& tx);
		void notifySyncProgress();
        void resetSystemState();

		virtual bool IsTestMode() { return false; }

        struct Cleaner
        {
            Cleaner(std::vector<wallet::Negotiator::Ptr>& t) : m_v{ t } {}
            ~Cleaner()
            {
                if (!m_v.empty())
                {
                    m_v.clear();
                }
            }
            std::vector<wallet::Negotiator::Ptr>& m_v;
        };

        template<typename Event>
        void process_event(const TxID& txId, Event&& event)
        {
            auto f = [txId, event = std::move(event), this]()
            {
                Cleaner cs{ m_removedNegotiators };
                if (auto it = m_negotiators.find(txId); it != m_negotiators.end())
                {
                    if (it->second->processEvent(event))
                    {
                        return;
                    }
                }
                LOG_DEBUG() << txId << " Unexpected event";
            };
            if (m_synchronized)
            {
                f();
            }
            else
            {
                m_pendingEvents.emplace_back(std::move(f));
            }
        }

        template<typename Message>
        void send_tx_message(const TxDescription& txDesc, Message&& msg)
        {
            msg.m_from = txDesc.m_myId;
            m_network->send_tx_message(txDesc.m_peerId, std::move(msg));
        }

    private:

        struct StateFinder;

        IKeyChain::Ptr m_keyChain;
        INetworkIO::Ptr m_network;
        std::map<TxID, wallet::Negotiator::Ptr>   m_negotiators;
        std::vector<wallet::Negotiator::Ptr>      m_removedNegotiators;
        TxCompletedAction m_tx_completed_action;
        std::deque<std::pair<TxID, Transaction::Ptr>> m_reg_requests;
        std::vector<std::pair<TxID, Transaction::Ptr>> m_pending_reg_requests;
        std::deque<Coin> m_pendingUtxoProofs;
        std::deque<wallet::Negotiator::Ptr> m_pendingKernelProofs;
        std::vector<Callback> m_pendingEvents;

		Block::SystemState::Full m_newState;
        Block::SystemState::ID m_knownStateID;
        std::unique_ptr<StateFinder> m_stateFinder;

        int m_syncDone;
        int m_syncTotal;
        bool m_synchronized;
        bool m_holdNodeConnection;

		std::vector<IWalletObserver*> m_subscribers;
    };
}
