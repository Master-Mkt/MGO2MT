#pragma once
#include "weapon_catalog.h"
#include <memory>

namespace mgo2mt::weapons {
// A selection draft is not an inventory grant, DP charge, or a spawn request.
// A missing authoritative context is distinct from DP disabled / zero balance.
class Selection {
 std::shared_ptr<const Catalog> catalog_;
 std::optional<SelectionContext> context_;
 std::array<std::optional<uint16_t>,3> selected_{};
public:
 explicit Selection(std::shared_ptr<const Catalog> catalog):catalog_(std::move(catalog)){}
 void context(std::optional<SelectionContext>);
 Access choose(Category,uint16_t);
 const Entry* selected(Category)const;
 std::vector<const Entry*> choices(Category)const;
 Access access_to(const Entry&)const;
 Quote quote()const;
 bool complete()const;
 const auto& current_context()const{return context_;}
};
}
