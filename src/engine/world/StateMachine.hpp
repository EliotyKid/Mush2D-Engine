#pragma once

#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Untyped view of a machine for tools (the F1 inspector lists the machines a GameObject registered).
class StateMachineBase {
public:
    virtual ~StateMachineBase() = default;
    virtual const char* current() const = 0; // "" before start
    virtual float timeInState() const = 0;
    // Set by GameObject::registerStateMachine: emits StateChanged on every change, start included.
    std::function<void(const char* from, const char* to)> registeredChange;
};

template<class Owner>
struct State {
    virtual ~State() = default;
    virtual const char* name() const = 0; // also the animation key
    virtual void enter(Owner&) {}
    virtual void exit(Owner&) {}
    virtual const char* update(Owner&, float dt) = 0; // nullptr = stay; otherwise the next state's name
};

// States by name. An owner may run several independent machines ("move", "action").
template<class Owner>
class StateMachine : public StateMachineBase {
public:
    std::function<void(const char* from, const char* to)> onChange; // animator, debug; start calls it with from = ""

    void add(std::unique_ptr<State<Owner>> state) { states_.push_back(std::move(state)); }

    // Enters the first state (no exit); onChange("", name) so an animator gets the first state.
    void start(Owner& owner, const char* name) {
        if (State<Owner>* s = find(name)) {
            current_ = s;
            time_ = 0.0f;
            s->enter(owner);
            if (onChange) onChange("", s->name());
            if (registeredChange) registeredChange("", s->name());
        }
    }

    // Runs the current state's update and applies at most one transition.
    void update(Owner& owner, float dt) {
        if (!current_) return;
        time_ += dt;
        if (const char* next = current_->update(owner, dt)) change(owner, next);
    }

    // Forced transition (damage, death); also the one update applies. Re-enters when name is the current state.
    void change(Owner& owner, const char* name) {
        State<Owner>* s = find(name);
        if (!s) return;
        const char* from = current_ ? current_->name() : "";
        if (current_) current_->exit(owner);
        current_ = s;
        time_ = 0.0f;
        s->enter(owner);
        if (onChange) onChange(from, s->name());
        if (registeredChange) registeredChange(from, s->name());
    }

    const char* current() const override { return current_ ? current_->name() : ""; }
    float timeInState() const override { return time_; }

private:
    // Unknown name: warns once per name on stderr (std::cerr); the caller keeps the state. nullptr: no state.
    State<Owner>* find(const char* name) {
        if (!name) return nullptr;
        for (const auto& s : states_)
            if (std::string(s->name()) == name) return s.get();
        if (warned_.insert(name).second) std::cerr << "StateMachine: unknown state '" << name << "'\n";
        return nullptr;
    }

    std::vector<std::unique_ptr<State<Owner>>> states_;
    State<Owner>* current_ = nullptr;
    float time_ = 0.0f;
    std::set<std::string> warned_;
};

// Runs a state written for Base (PatrolState for every enemy) in a machine of a derived Owner.
template<class Base, class Owner>
    requires std::derived_from<Owner, Base>
std::unique_ptr<State<Owner>> shared(std::unique_ptr<State<Base>> state) {
    struct Adapter : State<Owner> {
        std::unique_ptr<State<Base>> inner;
        explicit Adapter(std::unique_ptr<State<Base>> s) : inner(std::move(s)) {}
        const char* name() const override { return inner->name(); }
        void enter(Owner& o) override { inner->enter(o); }
        void exit(Owner& o) override { inner->exit(o); }
        const char* update(Owner& o, float dt) override { return inner->update(o, dt); }
    };
    return std::make_unique<Adapter>(std::move(state));
}
