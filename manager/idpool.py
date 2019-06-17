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

    def get_id(self, condition=lambda _: True):
        filtered_free = [id_ for id_ in self.ids_free if condition(id_)]
        if len(filtered_free) > 0:
            id_ = filtered_free[0]
            self.ids_free.remove(id_)
        else:
            id_ = self.new_id(self.last_id)
            self.last_id = id_
            while not condition(id_):
                prev = id_
                id_ = self.new_id(self.last_id)
                self.last_id = id_
                self.ids_free.add(prev)
            if self.max_id and id_ >= self.max_id:
                raise OSError(errno.ENOSPC)
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
