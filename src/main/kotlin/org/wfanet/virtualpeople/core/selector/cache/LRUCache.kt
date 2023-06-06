package src.main.kotlin.org.wfanet.virtualpeople.core.selector.cache

class LRUCache<K,V>(private val capacity: Int) : LinkedHashMap<K,V>(capacity) {

  override fun removeEldestEntry(eldest: MutableMap.MutableEntry<K, V>?): Boolean {
    return size > capacity
  }

}
