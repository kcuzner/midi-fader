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

import argparse, textwrap, logging, os, inspect, traceback
from itertools import chain
from collections import namedtuple, OrderedDict
import xml.etree.ElementTree as ET

###############################################################################
# Descriptor items
###############################################################################

DescriptorTag = namedtuple('DescriptorTag', ['tag'])
NestedType = namedtuple('NestedType', ['type'])

def handles_tag(tag):
    """
    Attaches a tag to a class for parsing purposes
    """
    def decorator(cls):
        if not inspect.isclass(cls):
            raise ValueError("The @handles_tag decorator is only valid for classes")
        attrname_format = '__tagname{}'
        index = 0
        while hasattr(cls, attrname_format.format(index)):
            index += 1
        setattr(cls, attrname_format.format(index), DescriptorTag(tag))
        return cls
    return decorator

def child_of(tag_cls):
    """
    Declares that this is a child of the passed tag handler class
    """
    def decorator(cls):
        if not inspect.isclass(cls):
            raise ValueError("The @child_of decorator is only valid for classes")
        attrname_format = '__child_tag{}'
        index = 0
        while hasattr(tag_cls, attrname_format.format(index)):
            index += 1
        setattr(tag_cls, attrname_format.format(index), NestedType(cls))
        return cls
    return decorator

def attrib_bool(el, attr_name):
    """
    Determines if the boolean attribute is set or cleared
    """
    return attr_name in el.attrib and el.attrib[attr_name] == attr_name

class TagHandler(object):
    """
    Base object which handles a tag and nested tags
    """
    logger = logging.getLogger('TagHandler')
    def parse(el, parent=None):
        TagHandler.logger.info("visiting {}".format(el))
        handled = False
        for c in TagHandler.subclasses():
            if c.match_tag(el):
                handled = True
                yield c(el, parent)
        if not handled:
            TagHandler.logger.warn('Element {} was not handled'.format(el))

    @classmethod
    def subclasses(cls):
        all_cls = []
        for c in cls.__subclasses__():
            all_cls.append(c)
            all_cls.extend(c.subclasses())
        return all_cls

    @classmethod
    def match_tag(cls, el):
        """
        Returns whether or not this handler can handle the passed tag
        """
        for attr_name in dir(cls):
            attr = getattr(cls, attr_name)
            if isinstance(attr, DescriptorTag) and attr.tag == el.tag:
                return True
        return False

    @classmethod
    def match_child(cls, el):
        """
        Returns whether or not the passed element is a valid child of the
        passed tag handler
        """
        for attr_name in dir(cls):
            attr = getattr(cls, attr_name)
            if isinstance(attr, NestedType) and attr.type.match_tag(el):
                return True
        return False
    def __init__(self, el, parent=None):
        if not type(self).match_tag(el):
            raise ValueError('Type {} cannot handle {}'.format(type(self), el))
        # all tags may have an ID
        self.id = el.attrib['id'] if 'id' in el.attrib else None
        self.children = []
        for e in el:
            if self.match_child(e):
                self.children.extend(TagHandler.parse(e, self))
            else:
                raise ValueError('{}: Unexpected child {}'.format(el, e))

    def post_parse(self, descriptor_collection):
        """
        Handles post-parse events in this handler and others following
        """
        for c in self.children:
            c.post_parse(descriptor_collection)

@handles_tag('descriptor')
class Descriptor(TagHandler):
    """
    Descriptor class. This represents the bare minimum required of a USB
    descriptor. By itself, no content is created.
    """
    def __init__(self, el, parent=None):
        if parent is not None:
            raise ValueError('A descriptor may not be the child of any element')
        self.type = int(el.attrib['type'], 0)
        self.top = ('childof' not in el.attrib) or attrib_bool(el, 'top')
        self.first = attrib_bool(el, 'first')
        self.parentid = el.attrib['childof'] if 'childof' in el.attrib else None
        self.wIndex = int(el.attrib['wIndex'], 0) if 'wIndex' in el.attrib else 0
        super().__init__(el, parent)
    @property
    def index(self):
        assert hasattr(self, '_index')
        return self._index
    @index.setter
    def index(self, i):
        self._index = i
    def to_source(self):
        return os.linesep.join([c.to_source() for c in self.children if hasattr(c, 'to_source')])
    def __repr__(self):
        return '<{}: Id: {}, Type: {}>'.format(type(self), self.id, self.type)

class BinaryContent(TagHandler):
    """
    Base binary content, generally not useful since by default it outputs
    nothing other than a comment which contins a name
    """
    def __init__(self, el, parent=None):
        self.name = el.attrib['name']
        super().__init__(el, parent)
    def __iter__(self):
        """
        Returns an iterator for this binary content. Each item should be
        a single uint8 (or a comma-separated list of uint8s) so that the
        generated source compiles, but it may be a string or whatever is needed
        """
        raise NotImplementedError
        yield
    def __len__(self):
        """
        Returns the length of this binary content in bytes. This does not need
        to correspond to the length of the sequence in __iter__
        """
        raise NotImplementedError
    def to_source(self):
        source = list(['{},'.format(b) for b in self])
        if len(source):
            source[0] += ' //' + self.name
        return os.linesep.join(source)

class SizedContent(BinaryContent):
    """
    Binary content which has an explicit size between 1 and 4 bytes

    This can have a content function which is invoked when the item is
    iterated, or it can take its content from the text of the element
    """
    def __init__(self, el, parent=None, size=None, contentfn=None):
        self.size = int(el.attrib['size'], 0) if size is None else size
        self.contentfn = contentfn if contentfn else lambda: el.text
        super().__init__(el, parent)
    def __len__(self):
        return self.size
    def __iter__(self):
        content = self.contentfn()
        parts = list(['((({}) >> {}) & 0xFF)'.format(content, i*8) for i in range(0, self.size)])
        if len(parts):
            yield ', '.join(parts)

@child_of(Descriptor)
@handles_tag('hidden')
class HiddenContent(SizedContent):
    """
    Sized content which has a name and content, but does not generate any source
    """
    def __init__(self, el, parent=None):
        if not el.text:
            raise ValueError('HiddenContent expects a non-empty element text')
        super().__init__(el, parent)
    def to_source(self):
        return ''

@child_of(Descriptor)
@handles_tag('byte')
class ByteContent(SizedContent):
    """
    Binary content for a single constant byte
    """
    def __init__(self, el, parent=None):
        if not el.text:
            raise ValueError('ByteContent expects a non-empty element text')
        super().__init__(el, parent, size=1)

@child_of(Descriptor)
@handles_tag('word')
class WordContent(SizedContent):
    """
    Binary content for a two constant bytes
    """
    def __init__(self, el, parent=None):
        if not el.text:
            raise ValueError('WordContent expects a non-empty element text')
        self.content = el.text
        super().__init__(el, parent, size=2)

@child_of(Descriptor)
@handles_tag('string')
class StringContent(BinaryContent):
    """
    String constant content
    """
    def __init__(self, el, parent=None):
        if not el.text:
            raise ValueError('StringContent expects a non-empty element text')
        self.bytes = el.text.encode('utf_16_le')
        super().__init__(el, parent)
    def __iter__(self):
        for word in ['{}, 0x{:02X}'.format("'{}'".format(chr(self.bytes[i])) if self.bytes[i+1] == 0 else hex(self, bytes[i]), self.bytes[i+1])
                for i in range(0, len(self.bytes), 2)]:
            yield word
    def __len__(self):
        return len(self.bytes)

@child_of(Descriptor)
@handles_tag('length')
class DescriptorLength(SizedContent):
    """
    Generates content containing the length of the parent descriptor
    """
    def __init__(self, el, parent):
        #TODO: Make this require a parent and count the length of the parent plus all descriptors which claim it as a parent
        # (this is the "all" attribute)
        self.all = attrib_bool(el, 'all')
        self.parent = parent
        super().__init__(el, parent, contentfn=self.parent_length)
    def parent_length(self):
        length = sum([len(c) for c in self.parent.children
            if hasattr(c, '__len__') and (self.all or not isinstance(c, ChildrenContent))])
        return length

@child_of(Descriptor)
@handles_tag('type')
class DescriptorType(SizedContent):
    """
    Generates content containing the type of the parent descriptor
    """
    def __init__(self, el, parent):
        self.parent = parent
        super().__init__(el, parent, contentfn=self.parent_type)
    def parent_type(self):
        return self.parent.type

@child_of(Descriptor)
@handles_tag('ref')
class DescriptorRef(SizedContent):
    """
    Generates content containing the index of another descriptor
    """
    def __init__(self, el, parent=None):
        self.type = int(el.attrib['type'], 0)
        #TODO: Get rid of type, there's no need
        self.refid = el.attrib['refid']
        super().__init__(el, parent, contentfn=self.index)
    def index(self):
        return self.__index
    def post_parse(self, descriptor_collection):
        super().post_parse(descriptor_collection)
        self.__index = descriptor_collection.find_by_id(self.refid).index

@child_of(Descriptor)
@handles_tag('index')
class DescriptorIndex(SizedContent):
    """
    Generates content containing the parent descriptor's index
    """
    def __init__(self, el, parent):
        self.parent = parent
        super().__init__(el, parent, contentfn=self.index)
    def index(self):
        return self.__index
    def post_parse(self, descriptor_collection):
        super().post_parse(descriptor_collection)
        self.__index = self.parent.index

@child_of(Descriptor)
@handles_tag('count')
class DescriptorCount(SizedContent):
    """
    Generates content containing the total number of some type of descriptor.
    """
    def __init__(self, el, parent=None):
        self.parent = parent
        self.associated = attrib_bool(el, 'associated')
        self.type = int(el.attrib['type'], 0)
        super().__init__(el, parent, contentfn=self.count)
    def count(self):
        return self.__count
    def post_parse(self, descriptor_collection):
        super().post_parse(descriptor_collection)
        descriptors = [d for d in descriptor_collection.find_by_type(self.type)
                if not self.associated or self.parent is None or d.parentid == self.parent.id]
        self.__count = len(descriptors)

@child_of(Descriptor)
@handles_tag('foreach')
class ForeachDescriptor(TagHandler):
    """
    Iterates descriptors of a particular type and generates content from them
    """
    def __init__(self, el, parent):
        self.parent = parent
        self.associated = attrib_bool(el, 'associated')
        self.type = int(el.attrib['type'], 0)
        super().__init__(el, parent)
    def post_parse(self, descriptor_collection):
        self.child_descriptors = [d for d in descriptor_collection.find_by_type(self.type)\
                if self.parent != d and (not self.associated or d.parentid == self.parent.id)]
    def __len__(self):
        return sum([len(c) for c in self.children if hasattr(c, '__len__')])
    def to_source(self):
        return os.linesep.join([line for c in self.children for line in c])


@child_of(ForeachDescriptor)
@handles_tag('echo')
class EchoContent(BinaryContent):
    """
    Echoes binary content of a particular name from a descriptor
    """
    def __init__(self, el, parent):
        self.parent = parent
        super().__init__(el, parent)
    def __to_echo(self):
        return [c for d in self.parent.child_descriptors\
                for c in d.children if isinstance(c, BinaryContent) and c.name == self.name]
    def __iter__(self):
        for d in self.__to_echo():
            for line in d:
                yield line
    def __len__(self):
        return sum([len(d) for d in self.__to_echo()])

@child_of(Descriptor)
@handles_tag('children')
class ChildrenContent(TagHandler):
    """
    Iterates descriptors which claim our parent as theirs and generates
    content from them
    """
    def __init__(self, el, parent):
        self.parent = parent
        self.type = int(el.attrib['type'], 0)
        super().__init__(el, parent)
    def post_parse(self, descriptor_collection):
        super().post_parse(descriptor_collection)
        all_of_type = descriptor_collection.find_by_type(self.type)
        self.child_descriptors = [d for d in all_of_type if d.parentid == self.parent.id]
    def __len__(self):
        return sum([len(c) for d in self.child_descriptors for c in d.children if hasattr(c, '__len__')])
    def __iter__(self):
        for d in self.child_descriptors:
            yield '/** Descriptor "{}" (type: {}) begin **/'.format(d.id, d.type)
            # Indent the child content slightly
            yield '  ' + d.to_source().replace('\n', '\n  ')
            yield '/** Descriptor "{}" (type: {}) end **/'.format(d.id, d.type)
    def to_source(self):
        return os.linesep.join(self)


class Endpoint(SizedContent):
    """
    Generates an endpoint address
    """
    def __init__(self, el, parent=None, dir_in=False):
        self.dir_in = dir_in
        super().__init__(el, parent, size=1, contentfn=self.endpoint)
    def endpoint(self):
        return '{:#04x}'.format(self.__endpoint | (0x80 if self.dir_in else 0))
    def post_parse(self, descriptor_collection):
        super().post_parse(descriptor_collection)
        self.__endpoint = descriptor_collection.reserve_endpoint()

@child_of(Descriptor)
@handles_tag('inendpoint')
class InEndpoint(Endpoint):
    """
    Generates the endpoint address for an IN endpoint
    """
    def __init__(self, el, parent=None):
        super().__init__(el, parent, True)

@child_of(Descriptor)
@handles_tag('outendpoint')
class OutEndpoint(Endpoint):
    """
    Generates an endpoint address for an OUT endpoint
    """
    def __init__(self, el, parent=None):
        super().__init__(el, parent, False)

@child_of(Descriptor)
@handles_tag('raw')
class RawContent(TagHandler):
    """
    Generates raw text content
    """
    def __init__(self, el, parent=None):
        self.raw = el.text
        super().__init__(el, parent)
    def to_source(self):
        return self.raw

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
        for d in TagHandler.parse(el):
            if d.type not in self.descriptors:
                self.descriptors[d.type] = [d]
            else:
                self.descriptors[d.type].append(d)
    
    def __iter__(self):
        for n, arry in self.descriptors.items():
            for d in arry:
                yield n, d

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
    def __init__(self, builder, max_endpoints=8):
        self.__endpoint = 0
        self.max_endpoints = max_endpoints
        self.top = {}
        self.by_id = {}
        self.by_type = {}
        self.indexes = {}
        for typenum, desc in builder:
            # Handle top-level descriptors
            if desc.top:
                if typenum not in self.top:
                    self.top[typenum] = [desc]
                else:
                    self.top[typenum].append(desc)
            # Index by id
            if desc.id in self.by_id:
                raise BadDefinitionError('Duplicate descriptor id {}'.format(desc.id))
            self.by_id[desc.id] = desc
            # Index by type
            if typenum not in self.by_type:
                self.by_type[typenum] = [desc]
            else:
                self.by_type[typenum].append(desc)
            # Assign indexes
            if typenum not in self.indexes:
                self.indexes[typenum] = 0
            desc.index = self.indexes[typenum]
            self.indexes[typenum] += 1
        for typenum, desc in builder:
            # Handle post-parse events
            desc.post_parse(self)

        
    def find_by_id(self, idname):
        return self.by_id[idname] if idname in self.by_id else None

    def find_by_type(self, typenum):
        return self.by_type[typenum] if typenum in self.by_type else []

    def reserve_endpoint(self):
        if self.__endpoint >= self.max_endpoints:
            raise BadDefinitionError("Maximum number of endpoints exceeded")
        endp = self.__endpoint
        self.__endpoint += 1
        return endp

    @property
    def device(self):
        return self.__device

    def to_source_iter(self):
        for typenum, descriptors in self.top.items():
            for d in descriptors:
                yield 'static USB_DATA_ALIGN uint8_t {}[] = {{'.format(d.id)
                # indent the descriptor slightly
                yield '  ' + d.to_source().replace('\n', '\n  ')
                yield '};\n'
        yield 'const USBDescriptorEntry usb_descriptors[] = {'
        for typenum, descriptors in self.top.items():
            for d in descriptors:
                yield '  {{ 0x{:02x}{:02x}, {:#06x}, sizeof({}), {} }},'\
                        .format(d.type, d.index, d.wIndex, d.id, d.id)
        yield '  { 0x0000, 0x0000, 0x00, NULL }'
        yield '};'

    def to_source(self):
        """
        Creates source code for our descriptors
        """
        return os.linesep.join(self.to_source_iter())


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

def extract_elements(fragment, fname=None):
    """
    Extracts possible descriptor elements from the passed fragment

    Returns an interable
    """
    try:
        return ET.fromstring('<root>' + fragment + '</root>')
    except ET.ParseError as e:
        source_lines = fragment.splitlines()
        max_lines = str(len(source_lines))
        numbers = [str(i).ljust(len(max_lines)) + ' |' for i in range(1, len(source_lines)+1)]
        numbered_lines = list([''.join(t) for t in zip(numbers, source_lines)])
        min_line = max(1, e.position[0]-3) - 1
        max_line = min(len(source_lines)+1, e.position[0]+3) - 1
        fname = fname + os.linesep if fname else ''
        e.xml_source = fname + os.linesep.join(numbered_lines[min_line:max_line])
        raise

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

    builder = DescriptorCollectionBuilder()
    for f in args.files:
        elements = [el for c in extract_c_comments(f) for el in extract_elements(c, f)]
        for el in elements:
            builder.add_descriptors(el)
    descriptors = builder.compile()
    print(descriptors.to_source())

if __name__ == '__main__':
    try:
        main()
    except ET.ParseError as e:
        print(traceback.format_exc())
        if hasattr(e, 'xml_source'):
            print(e.xml_source)

