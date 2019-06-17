# https://gist.github.com/sunetos/1068329

__author__ = 'Adam R. Smith'

import errno

class IDPool(object):
    '''
    Create a pool of IDs to allow reuse. The "new_id" function generates the next
    valid ID from the previous one. If not given, defaults to incrementing an integer.
    '''

    def __init__(self, min_id=-1, max_id=None, new_id=None):
        self.max_id = max_id
        if new_id is None: new_id = lambda x: x + 1

        self.ids_in_use = set()
        self.ids_free = set()
        self.new_id = new_id
        self.last_id = min_id

    def get_id(self):
        if len(self.ids_free) > 0:
            id_ = self.ids_free.pop()
        else:
            id_ = self.new_id(self.last_id)
            if self.max_id and id_ >= self.max_id:
                raise OSError(errno.ENOSPC)
        self.last_id = id_
        self.ids_in_use.add(id_)
        return id_

    def release_id(self, the_id):
        if the_id in self.ids_in_use:
            self.ids_in_use.remove(the_id)
            self.ids_free.add(the_id)

if __name__ == '__main__':
    pool = IDPool()
    for i in range(5):
        print('got id: %d' % pool.get_id())
    print('releasing id 3')
    pool.release_id(3)
    print('got id: %d' % pool.get_id())
    print('got id: %d' % pool.get_id())
