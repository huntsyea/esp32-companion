#ifndef CONVERSATION_RECOVERY_POLICY_H
#define CONVERSATION_RECOVERY_POLICY_H

inline bool ShouldCloseConversationOnNetworkLoss(bool connecting, bool listening, bool speaking,
                                                 bool conversation_active) {
    return connecting || listening || speaking || conversation_active;
}

#endif  // CONVERSATION_RECOVERY_POLICY_H
