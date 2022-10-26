# Based on https://github.com/vadimcn/vscode-lldb/blob/master/formatters/rust.py
from __future__ import print_function, division
import sys
import logging
import re
import lldb

if sys.version_info[0] == 2:
    # python2-based LLDB accepts utf8-encoded ascii strings only.
    def to_lldb_str(s): return s.encode(
        'utf8', 'backslashreplace') if isinstance(s, unicode) else s
    range = xrange
else:
    to_lldb_str = str

string_encoding = "escape"  # remove | unicode | escape

log = logging.getLogger(__name__)

module = sys.modules[__name__]
d_category = None


def __lldb_init_module(debugger, dict):
    global d_category

    d_category = debugger.CreateCategory('D')
    d_category.SetEnabled(True)

    attach_synthetic_to_type(
        UtilsSlicePrinter, r'^tint::utils::Slice<.+>$', True)

    attach_synthetic_to_type(
        UtilsVectorPrinter, r'^tint::utils::Vector<.+>$', True)

    attach_synthetic_to_type(
        UtilsVectorRefPrinter, r'^tint::utils::VectorRef<.+>$', True)


def attach_synthetic_to_type(synth_class, type_name, is_regex=False):
    global module, d_category
    synth = lldb.SBTypeSynthetic.CreateWithClassName(
        __name__ + '.' + synth_class.__name__)
    synth.SetOptions(lldb.eTypeOptionCascade)
    ret = d_category.AddTypeSynthetic(
        lldb.SBTypeNameSpecifier(type_name, is_regex), synth)
    log.debug('attaching synthetic %s to "%s", is_regex=%s -> %s',
              synth_class.__name__, type_name, is_regex, ret)

    def summary_fn(valobj, dict): return get_synth_summary(
        synth_class, valobj, dict)
    # LLDB accesses summary fn's by name, so we need to create a unique one.
    summary_fn.__name__ = '_get_synth_summary_' + synth_class.__name__
    setattr(module, summary_fn.__name__, summary_fn)
    attach_summary_to_type(summary_fn, type_name, is_regex)


def attach_summary_to_type(summary_fn, type_name, is_regex=False):
    global module, d_category
    summary = lldb.SBTypeSummary.CreateWithFunctionName(
        __name__ + '.' + summary_fn.__name__)
    summary.SetOptions(lldb.eTypeOptionCascade)
    ret = d_category.AddTypeSummary(
        lldb.SBTypeNameSpecifier(type_name, is_regex), summary)
    log.debug('attaching summary %s to "%s", is_regex=%s -> %s',
              summary_fn.__name__, type_name, is_regex, ret)


def get_synth_summary(synth_class, valobj, dict):
    ''''
    get_summary' is annoyingly not a part of the standard LLDB synth provider API.
    This trick allows us to share data extraction logic between synth providers and their sibling summary providers.
    '''
    synth = synth_class(valobj.GetNonSyntheticValue(), dict)
    synth.update()
    summary = synth.get_summary()
    return to_lldb_str(summary)


class Printer(object):
    '''Base class for Printers'''

    def __init__(self, valobj, dict={}):
        self.valobj = valobj
        self.initialize()

    def initialize(self):
        return None

    def update(self):
        return False

    def num_children(self):
        return 0

    def has_children(self):
        return False

    def get_child_at_index(self, index):
        return None

    def get_child_index(self, name):
        return None

    def get_summary(self):
        return None

    def member(self, *chain):
        '''Performs chained GetChildMemberWithName lookups'''
        valobj = self.valobj
        for name in chain:
            valobj = valobj.GetChildMemberWithName(name)
        return valobj

    def template_params(self):
        '''Returns list of template params values (as strings)'''
        type_name = self.valobj.GetTypeName()
        params = []
        level = 0
        start = 0
        for i, c in enumerate(type_name):
            if c == '<':
                level += 1
                if level == 1:
                    start = i + 1
            elif c == '>':
                level -= 1
                if level == 0:
                    params.append(type_name[start:i].strip())
            elif c == ',' and level == 1:
                params.append(type_name[start:i].strip())
                start = i + 1
        return params

    def template_param_at(self, index):
        '''Returns template param value at index (as string)'''
        return self.template_params()[index]


class UtilsSlicePrinter(Printer):
    '''Printer for tint::utils::Slice<T>'''

    def initialize(self):
        self.len = self.valobj.GetChildMemberWithName('len')
        self.cap = self.valobj.GetChildMemberWithName('cap')
        self.data = self.valobj.GetChildMemberWithName('data')
        self.elem_type = self.data.GetType().GetPointeeType()
        self.elem_size = self.elem_type.GetByteSize()

    def get_summary(self):
        return 'length={} capacity={}'.format(self.len.GetValueAsUnsigned(), self.cap.GetValueAsUnsigned())

    def num_children(self):
        # NOTE: VS Code on MacOS hangs if we try to expand something too large, so put an artificial limit
        # until we can figure out how to know if this is a valid instance.
        return min(self.len.GetValueAsUnsigned(), 256)

    def has_children(self):
        return True

    def get_child_at_index(self, index):
        try:
            if not 0 <= index < self.num_children():
                return None
            offset = index * self.elem_size
            return self.data.CreateChildAtOffset('[%s]' % index, offset, self.elem_type)
        except Exception as e:
            log.error('%s', e)
            raise


class UtilsVectorPrinter(Printer):
    '''Printer for tint::utils::Vector<T, N>'''

    def initialize(self):
        self.slice = self.member('impl_', 'slice')
        self.slice_printer = UtilsSlicePrinter(self.slice)
        self.fixed_size = int(self.template_param_at(1))
        self.cap = self.slice_printer.member('cap')

    def get_summary(self):
        using_heap = self.cap.GetValueAsUnsigned() > self.fixed_size
        return 'heap={} {}'.format(using_heap, self.slice_printer.get_summary())

    def num_children(self):
        return self.slice_printer.num_children()

    def has_children(self):
        return self.slice_printer.has_children()

    def get_child_at_index(self, index):
        return self.slice_printer.get_child_at_index(index)


class UtilsVectorRefPrinter(Printer):
    '''Printer for tint::utils::VectorRef<T>'''

    def initialize(self):
        self.slice = self.member('slice_')
        self.slice_printer = UtilsSlicePrinter(self.slice)
        self.can_move = self.member('can_move_')

    def get_summary(self):
        return 'can_move={} {}'.format(self.can_move.GetValue(), self.slice_printer.get_summary())

    def num_children(self):
        return self.slice_printer.num_children()

    def has_children(self):
        return self.slice_printer.has_children()

    def get_child_at_index(self, index):
        return self.slice_printer.get_child_at_index(index)
