#!/usr/bin/env python3
# Copyright 2023 The Dawn Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""
A basic intermediate representation between dawn.json and protobuf
accesses
"""


class LPMIR():
    def __init__(self, member, proto_access) -> None:
        self._member = member
        self._protobuf_access = proto_access

    @property
    def member_name(self):
        return as_protobufMemberNameLPM(self._member.name)

    @property
    def overrides_key(self):
        return self._member.name.canonical_case()

    @property
    def member(self):
        return self._member

    @property
    def protobuf_access(self):
        if self._protobuf_access:
            return self._protobuf_access
        return ""


class CmdState():
    def __init__(self, command, member) -> None:
        self.stack = [LPMIR(member, None)]
        self._command = command

    @property
    def command(self):
        return self._command

    @property
    def overrides_key(self):
        return self._command.name.canonical_case()

    @property
    def protobuf_command_name(self):
        return self._command.name.concatcase()

    # Insert an 'access' at the top of the stack
    def insert_top_access(self, access):
        tmp = self.stack.pop()
        self.stack.append(LPMIR(tmp.member, access))

    def push(self, member, access):
        self.stack.append(LPMIR(member, access))

    def pop(self):
        return self.stack.pop()

    def top(self):
        return self.stack[-1]

    def is_empty(self):
        return len(self.stack)

    def size(self):
        return len(self.stack)

    def __iter__(self):
        return self.StackIterator(self.stack)

    class StackIterator:
        def __init__(self, stack):
            self.stack = stack
            self.index = 0

        def __next__(self):
            if self.index < len(self.stack):
                item = self.stack[self.index]
                self.index += 1
                return item
            else:
                raise StopIteration


#############################################################
# Generators
#############################################################


def as_cppMemberName(*names):
    return names[0].camelCase() + ''.join(
        [name.CamelCase() for name in names[1:]])


def as_protoMemberName(*names):
    return names[0].camelCase() + ''.join(
        [name.CamelCase() for name in names[1:]])


# Helper to generate member accesses within C++ of protobuf objects
# example: cmd.membera().memberb()
def as_protobufMemberNameLPM(*names):
    # `descriptor` is a reserved keyword in lib-protobuf-mutator
    if (names[0].concatcase() == "descriptor"):
        return "desc"
    return ''.join([name.concatcase().lower() for name in names])
