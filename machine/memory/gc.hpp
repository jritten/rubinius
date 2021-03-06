#ifndef RBX_VM_GC_HPP
#define RBX_VM_GC_HPP

#include <list>

#include "memory/header.hpp"

#include "shared_state.hpp"

#include "class/object.hpp"

namespace rubinius {
  struct CallFrame;

  class Memory;
  class VariableScope;
  class GlobalCache;
  class StackVariables;
  class LLVMState;

  namespace capi {
    class Handles;
  }

namespace memory {
  class ManagedThread;

  /**
   * Holds all the root pointers from which garbage collections will commence.
   * This includes the globally accessible Ruby objects such as class and
   * module instances, global variables, etc, but also various handles that
   * are used for FFI and CAPI.
   */

  class GCData {
    Roots& roots_;
    capi::Handles* handles_;
    std::list<capi::Handle*>* cached_handles_;
    GlobalCache* global_cache_;
    ThreadNexus* thread_nexus_;
    std::list<capi::GlobalHandle*>* global_handle_locations_;

  public:
    GCData(VM*);

    Roots& roots() {
      return roots_;
    }

    ThreadNexus* thread_nexus() {
      return thread_nexus_;
    }

    capi::Handles* handles() {
      return handles_;
    }

    std::list<capi::Handle*>* cached_handles() {
      return cached_handles_;
    }

    GlobalCache* global_cache() {
      return global_cache_;
    }

    std::list<capi::GlobalHandle*>* global_handle_locations() {
      return global_handle_locations_;
    }
  };

  class AddressDisplacement {
    intptr_t offset_;
    intptr_t lower_bound_;
    intptr_t upper_bound_;

  public:
    AddressDisplacement(intptr_t o, intptr_t l, intptr_t u)
      : offset_(o)
      , lower_bound_(l)
      , upper_bound_(u)
    {}

    template <typename T>
      T displace(T ptr) const {
        intptr_t addr = (intptr_t)ptr;
        if(addr < lower_bound_) return ptr;
        if(addr >= upper_bound_) return ptr;

        return (T)((char*)ptr + offset_);
      }
  };


  /**
   * Abstract base class for the various garbage collector implementations.
   * Defines the interface the VM will use to perform garbage collections,
   * as well as providing implementations of common methods such as
   * mark_object and scan_object.
   */

  class GarbageCollector {
  protected:
    /// Reference to the Memory we are collecting
    Memory* memory_;

  private:
    /// Array of weak references
    ObjectArray* weak_refs_;

  public:
    /**
     * Constructor; takes a pointer to Memory.
     */
    GarbageCollector(Memory *om);

    virtual ~GarbageCollector() {
      if(weak_refs_) delete weak_refs_;
    }

    /**
     * Subclasses implement appropriate behaviour for handling a live object
     * encountered during garbage collection.
     */
    virtual Object* saw_object(Object*) = 0;
    virtual void scanned_object(Object*) = 0;
    virtual bool mature_gc_in_progress() = 0;

    // Scans the specified Object for references to other Objects.
    void scan_object(Object* obj);
    void delete_object(Object* obj);
    void walk_call_frame(CallFrame* call_frame, AddressDisplacement* offset=0);
    void verify_call_frame(CallFrame* call_frame, AddressDisplacement* offset=0);
    void saw_variable_scope(CallFrame* call_frame, StackVariables* scope);
    void verify_variable_scope(CallFrame* call_frame, StackVariables* scope);

    /**
     * Marks the specified Object +obj+ as live.
     */
    Object* mark_object(Object* obj) {
      if(!obj || !obj->reference_p()) return obj;
      Object* tmp = saw_object(obj);
      if(tmp) return tmp;
      return obj;
    }

    void clean_weakrefs(bool check_forwards=false);
    void clean_locked_objects(ManagedThread* thr, bool young_only);

    // Scans the thread for object references
    void scan(ManagedThread* thr, bool young_only);
    void scan(VariableRootBuffers& buffers, bool young_only, AddressDisplacement* offset=0);
    void scan(RootBuffers& rb, bool young_only);

    void verify(GCData* data);

    VM* vm();
    Memory* object_memory() {
      return memory_;
    }

    /**
     * Adds a weak reference to the specified object.
     *
     * A weak reference provides a way to hold a reference to an object without
     * that reference being sufficient to keep the object alive. If no other
     * reference to the weak-referenced object exists, it can be collected by
     * the garbage collector, with the weak-reference subsequently returning
     * null.
     */
    void add_weak_ref(Object* obj) {
      if(!weak_refs_) {
        weak_refs_ = new ObjectArray;
      }

      weak_refs_->push_back(obj);
    }

    void reset_stats() {
    }

    ObjectArray* weak_refs_set() {
      return weak_refs_;
    }

    friend class ObjectMark;
  };
}
}

#endif
