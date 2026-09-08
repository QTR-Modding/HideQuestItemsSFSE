#pragma once

namespace RE
{
    class PlayerInventoryDataModel;
}

namespace HideQuestItems::Runtime
{
    using PlayerInventoryReconcile = void (*)(RE::PlayerInventoryDataModel*, bool);

    void ReconcileWithHiddenQuestItems(
        RE::PlayerInventoryDataModel* a_model,
        bool a_incremental,
        PlayerInventoryReconcile a_reconcile);
}
