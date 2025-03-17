class cache:
    def __init__(self, f):
        self.f = f
        self.cache = {}
        # Add function attributes for better introspection
        self.__name__ = f.__name__
        self.__doc__ = f.__doc__
        self.__module__ = f.__module__
    
    def __call__(self, *args):
        # Use immutable keys for cache dictionary
        # Convert args to tuple if it's not already
        cache_key = args if isinstance(args, tuple) else tuple(args)
        
        if cache_key not in self.cache:
            self.cache[cache_key] = self.f(*args)
        return self.cache[cache_key]
    
    def clear_cache(self):
        """Clear the cache dictionary"""
        self.cache.clear()
    
def reduce(function, sequence, initial=...):
    it = iter(sequence)
    
    if initial is ...:
        try:
            value = next(it)
        except StopIteration:
            raise TypeError("reduce() of empty sequence with no initial value")
    else:
        value = initial
    
    for element in it:
        value = function(value, element)
    
    return value

class partial:
    def __init__(self, f, *args, **kwargs):
        if not callable(f):
            raise TypeError("the first argument must be callable")
        
        self.f = f
        self.args = args
        self.kwargs = kwargs.copy()  # Create a copy to avoid modifying the original
        
        # Copy attributes from the function for better introspection
        self.__name__ = getattr(f, '__name__', 'partial')
        self.__doc__ = getattr(f, '__doc__', None)
        self.__module__ = getattr(f, '__module__', None)
    
    def __call__(self, *args, **kwargs):
        new_kwargs = self.kwargs.copy()  # Create a copy to avoid modifying self.kwargs
        new_kwargs.update(kwargs)
        return self.f(*(self.args + args), **new_kwargs)
