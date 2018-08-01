#!/usr/bin/env python3
"""
This is a descriptor generator that locates descriptor definitions inside
passed files. It then generates both a C source and C header file which
contains the descriptor along with assigned endpoint addresses. It handles
auto-assignment of endpoint numbers as well.

Files are parsed in the order they are presented on the command line. This
affects the assigned endpoints along with the order in which items appear in
the descriptor.

Descriptors will be pulled from files between /* and */ patterns. These
patterns must appear on their own line, but can be preceded by any whitespace.
All leading "*" characters preceded by whitespace will be stripped from each
line.

"""

import argparse, textwrap, logging, os, inspect
from collections import namedtuple, OrderedDict
import xml.etree.ElementTree as ET

###############################################################################
# Descriptor items
###############################################################################

DescriptorTag = namedtuple('DescriptorTag', ['tag'])
DescriptorType = namedtuple('DescriptorType', ['type'])
NestedType = namedtuple('NestedType', ['type'])

class DescriptorItem(object):
    """
    Base class for descriptor items.

    This handles the translation of the data in the descriptor into C source
    code or C header variables
    """
    def __init__(self, name, length):
        self.name = name
        self.length = length
    def to_source(self, descriptor, all_descriptors):
        """
        Called when this descriptor item needs to be translated to C source
        """
        return "// {}".format(self.name)

class DescriptorItemDefinition(object):
    """
    Base class for descriptor item definitions

    When the item definition is called, it should produce an appropriate
    descriptor item.
    """
    def __init__(self, name):
        self.name = name
    def __call__(self, element):
        return DescriptorItem(self.name, 0)

class DynamicItem(DescriptorItem):
    """
    Item whose numerical value is determined when transformed into source

    It must have a constant length
    """
    def __init__(self, name, value_fn, length):
        super().__init__(name, length)
        if length < 1 or length > 4:
            raise ValueError("The {} supports values only between 1 and 4 bytes in length".format(type(self)))
        self.value_fn = value_fn
        self.length = length
    def to_source(self, descriptor, all_descriptors):
        value = self.value_fn(descriptor, add_descriptors)
        source = ['(({} >> {}) & 0xFF),'.format(self.value, n) for n in range(0, self.length)]
        return ' '.join(source) + ' ' + super().to_souce(descriptor, all_descriptors)

class ConstantItem(DynamicItem):
    """
    Item that is a constant derived directly from the XML element
    """
    def __init__(self, name, value, length):
        super().__init__(name, lambda d, ad: value, length)

class ByteItemDefinition(DescriptorItemDefinition):
    """
    Item that takes up exactly one byte and is derived directly from the XML
    element or has a provided value
    """
    def __init__(self, attribute, value=None):
        super().__init__(attribute)
        self.value = value
    def __call__(self, element):
        value = self.value if self.value is not None else element.attrib[self.name]
        return ConstantItem(self.name, value, 1)

class WordItemDefinition(DescriptorItemDefinition):
    """
    Item that takes up exactly two bytes and is derived directly from the XML
    element
    """
    def __init__(self, attribute, value=None):
        super().__init__(attribute)
        self.value = value
    def __call__(self, element):
        value = self.value if self.value is not None else element.attrib[self.name]
        return ConstantItem(self.name, value, 2)

class DescriptorTypeDefinition(DescriptorItemDefinition):
    """
    Item which produces a byte with the descriptor type for the current descriptor
    """
    def __init__(self, name='bDescriptorType'):
        super().__init__(name)
    def __call__(self, element):
        return DynamicItem(self.name, lambda d, ad: d.descriptor_type, 1)

class LengthItem(DescriptorItem):
    """
    Item that contains the total length of the descriptor in a single byte
    """
    def __init__(self, name):
        super().__init__(name, 1)
    def to_source(self, descriptor, all_descriptors):
        value = sum([i.length for i in descriptor.items])
        return '{}, '.format(value) + super().to_source(descriptor, all_descriptors)

class LengthItemDefinition(DescriptorItemDefinition):
    """
    Provides the total length of a descriptor
    """
    def __init__(self):
        super().__init__('bLength')
    def __call__(self, element):
        return LengthItem(self.name)

class TextItem(DescriptorItem):
    """
    Item that encodes text as UTF-16LE
    """
    def __init__(self, name, string):
        self.raw = string.encode('utf_16_le')
        super().__init__(name, len(self.raw))
    def to_source(self, descriptor, all_descriptors):
        chunks = list(['{}, 0x{:02X},'.format("'{}'".format(chr(self.raw[i])) if self.raw[i+1] == 0 else hex(self.raw[i]), self.raw[i+1])
                for i in range(0, len(self.raw), 2)])
        chunks[0] += ' ' + super().to_source(descriptor, all_descriptors)
        return os.linesep.join(chunks)

class TextItemDefinition(DescriptorItemDefinition):
    """
    Provides an item that encodes text as UTF-16LE
    """
    def __init__(self, name, value=None):
        super().__init__(name)
        self.value = value
    def __call__(self, element):
        value = self.value if self.value is not None else element.text
        return TextItem(self.name, value)

###############################################################################
# Descriptors
###############################################################################

class OrderedClassMembers(type):
    """
    Metaclass which provides an __ordered__ class-level attribute which
    contains the attributes in declaration order
    """
    @classmethod
    def __prepare__(cls, name, bases):
        return OrderedDict()

    def __new__(self, name, bases, classdict):
        classdict['__ordered__'] = [key for key in classdict.keys()
                if key not in ('__module__', '__qualname__')]
        return type.__new__(self, name, bases, classdict)

def descriptor_type(typenum):
    """
    Attaches a descriptor type to a class
    """
    def decorator(cls):
        if not inspect.isclass(cls):
            raise ValueError("The @descriptor_type decorator is only valid for classes")
        for name in dir(cls):
            attr = getattr(cls, name)
            if isinstance(attr, DescriptorType):
                raise ValueError("The decorated class already has a DescriptorType")
        cls.__descriptor_type = DescriptorType(typenum)
        return cls
    return decorator

def descriptor_tag(elname):
    """
    Attaches an element tag name to a class
    """
    def decorator(cls):
        if not inspect.isclass(cls):
            raise ValueError("The @descriptor_element decorator is only valid for classes")
        attrname_format = '__descriptor_element{}'
        index = 0
        while hasattr(cls, attrname_format.format(index)):
            index += 1
        setattr(cls, attrname_format.format(index), DescriptorTag(elname))
        return cls
    return decorator

def child_of(descriptor_cls):
    """
    Declares that this is a child of the passed descriptor class
    """
    def decorator(cls):
        if not inspect.isclass(cls):
            raise ValueError("The @child_of decorator is only valid for classes")
        attrname_format = '__child_type{}'
        index = 0
        while hasattr(descriptor_cls, attrname_format.format(index)):
            index += 1
        setattr(cls, attrname_format.format(index), NestedType(cls))
        return cls

class Descriptor(metaclass=OrderedClassMembers):
    """
    Base descriptor class, does not do anything other than feed down into its
    subclasses.

    Descriptors are matched against element tags by DescriptorTag instances on
    the class level. A Descriptor can have any number of tags, though most
    descriptors should really only have one.
    """
    logger = logging.getLogger('descriptor')
    def parse_descriptor_el(el):
        """
        Parses a descriptor element using a subtype of this type

        Returns a generator
        """
        no_match = True
        for c in Descriptor.__subclasses__():
            if c.match_tag(el):
                descriptor = c(el)
                yield descriptor
                for d in [Descriptor.parse_descriptor_el(e) for e in el]:
                    if not descriptor.is_child(d):
                        Descriptor.logger.warn('{} is not a valid child of {}, pushed to global scope'.format(d, descriptor))
                        yield d
                    else:
                        descriptor.add_child(d)

    @classmethod
    def match_tag(cls, el):
        """
        Returns whether or not this descriptor matches the passed element's
        tag.
        """
        for attr_name in dir(cls):
            attr = getattr(cls, attr_name)
            if isinstance(attr, DescriptorTag) and attr.tag == el.tag:
                return True
        return False

    def __init__(self, el):
        self.__items = []
        self.__children = []
        for i in self.__ordered__:
            attr = getattr(self, i)
            if isinstance(attr, DescriptorItemDefinition):
                self.__items.append(attr(el))

    @property
    def descriptor_type(self):
        for attr_name in dir(self):
            if attr_name == 'descriptor_type':
                continue
            attr = getattr(self, attr_name)
            if isinstance(attr, DescriptorType):
                return attr.type
        raise ValueError("Descriptor {} does not have a type".format(self))

    @property
    def items(self):
        return self.__items

    def is_child(self, descriptor):
        """
        Returns whether or not the passed descriptor could be a child of this
        descriptor
        """
        # should this be a classmethod instead? I only call it from an instance
        # but the data truly lives on the class level...
        for attr_name in dir(cls):
            attr = getattr(cls, attr_name)
            if isinstance(attr, NestedType) and isinstance(descriptor, attr.type):
                return True
        return False

    def add_child(self, descriptor):
        if not self.is_child(descriptor):
            raise ValueError('{} is not a valid child of {}'.format(descriptor, self))
        self.__children.append(descriptor)

    def to_source(self, all_descriptors):
        """
        Generates a source code fragment for this descriptor
        """
        return os.linesep.join([d.to_source(self, all_descriptors) for d in self.items])

@descriptor_tag('device')
@descriptor_type(0x01)
class DeviceDescriptor(Descriptor):
    bLength = LengthItemDefinition()
    bDescriptorType = DescriptorTypeDefinition()
    bcdUSB = WordItemDefinition('bcdUSB')
    bDeviceClass = ByteItemDefinition('bDeviceClass')
    bDeviceSubClass = ByteItemDefinition('bDeviceSubClass')
    bDeviceProtocol = ByteItemDefinition('bDeviceProtocol')
    bMaxPacketSize0 = ByteItemDefinition('bMaxPacketSize0')
    idVendor = WordItemDefinition('idVendor')
    idProduct = WordItemDefinition('idProduct')
    bcdDevice = WordItemDefinition('bcdDevice')

@descriptor_tag('string')
@descriptor_type(0x03)
class StringDescriptor(Descriptor):
    bLength = LengthItemDefinition()
    bDescriptorType = DescriptorTypeDefinition()
    wString = TextItemDefinition('wString')

###############################################################################
# Formatting and organization
###############################################################################

class DescriptorCollectionBuilder(object):
    """
    Collates descriptors from elements in preparation for compiling them
    into a collection.

    Rules/Caveats:
    - There can only be one device descriptor
    - There can only be one configuration descriptor
    - All interface descriptors belong to the one configuration descriptor
    - If there are any strings, the language descriptor will be automatically
      generated
    - All "id" attributes must be unique
    
    """
    def __init__(self):
        self.descriptors = {}
    def add_descriptors(self, el):
        """
        Adds descriptors to the collection from the passed element
        """
        for d in Descriptor.parse_descriptor_el(el):
            if d.descriptor_type not in self.descriptors:
                self.descriptors[d.descriptor_type] = [d]
            else:
                self.descriptors[d.descriptor_type].append(d)

    def find(self, descriptor_type):
        """
        Finds all descriptors of the passed type
        """
        typenum = descriptor_type.descriptor_type()
        if typenum not in self.descriptors:
            return []
        else:
            return self.descriptors[typenum]

    def compile(self):
        """
        Compiles this builder into a collection
        """
        return DescriptorCollection(self)

class BadDefinitionError(Exception):
    pass

class DescriptorCollection(object):
    """
    Collection of descriptors that are related.
    """
    def __init__(self, builder):
        # 1. Extract the device descriptor
        if len(builder.find(DeviceDescriptor)) != 1:
            raise BadDefinitionError('Expected exactly 1 device descriptor, found {}'.format(len(builder.find(DeviceDescriptor))))
        self.__device = builder.find(DeviceDescriptor)[0]
        # 2. Extract all other types of descriptors, of which there can be many

    @property
    def device(self):
        return self.__device


###############################################################################
# File parsing
###############################################################################

def extract_c_comments(filename):
    """
    Extracts the contents of C comments from the passed file
    """
    with open(filename) as f:
        gathering = False
        gathered = None
        for line in f:
            stripped = line.lstrip()
            if not gathering and stripped.startswith('/*'):
                gathering = True
            elif gathering and stripped.rstrip().endswith('*/'):
                gathering = False
                yield gathered
                gathered = None
            elif gathering:
                no_stars = stripped.lstrip('*')
                gathered = gathered + no_stars if gathered else no_stars

def extract_elements(fragment):
    """
    Extracts possible descriptor elements from the passed fragment

    Returns an interable
    """
    return ET.fromstring('<root>' + fragment + '</root>')

def main():
    parser = argparse.ArgumentParser(description='Parses USB descriptors into a C file',
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog=textwrap.dedent(__doc__))
    parser.add_argument('files', metavar='FILE', nargs='+', help='Files to parse')
    output_type = parser.add_mutually_exclusive_group(required=True)
    output_type.add_argument('-od', '--output-deps', help='Output dependency filename')
    output_type.add_argument('-os', '--output-src', help='Output source filename')
    output_type.add_argument('-oh', '--output-header', help='Output header filename')
    parser.add_argument('--max-endpoints', default=8, help='Maximum number of endpoints')

    args = parser.parse_args()

    descriptors = DescriptorCollectionBuilder()
    for f in args.files:
        elements = [el for c in extract_c_comments(f) for el in extract_elements(c)]
        for el in elements:
            descriptors.add_descriptors(el)
    print(descriptors.descriptors)

if __name__ == '__main__':
    main()

