# Copyright 2019 The Dawn Authors
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

from collections import namedtuple
from common import Name
from common import RecordMember
from common import CommandType
from common import linked_record_members

def concat_names(*namesOrStrings):
    def append_string(acc, curr):
        if isinstance(curr, Name):
            return acc + curr.chunks
        else:
            acc.append(curr)
            return acc
    names = reduce(append_string, namesOrStrings, [])
    return ' '.join(names)

# Create wire commands from api methods
def add_wire_commands(api_params, wire_json):
    wire_params = api_params.copy()
    types = wire_params['types']
    wire_params['cmd_records'] = {
      'command': [],
      'return command': []
    }

    def add_command(cmd):
        wire_params['cmd_records'][cmd.category].append(cmd)

    string_message_member = RecordMember(Name('message'), types['char'], 'const*', False, False)
    string_message_member.length = 'strlen'

    # Generate commands from object methods
    for api_object in wire_params['by_category']['object']:
        for method in api_object.methods:
            if method.return_type.category != 'object' and method.return_type.name.canonical_case() != 'void':
                # No other return types supported
                continue

            members = []

            # Create object method commands by prepending "self"
            members.append(RecordMember(Name('self'), types[api_object.dict_name], 'value', False, False))

            for arg in method.arguments:
                members.append(arg)

            # Client->Server commands that return an object return the result object handle
            if method.return_type.category == 'object':
                result = RecordMember(Name('result'), types['ObjectHandle'], 'value', False, True)
                result.set_target_type(method.return_type)
                members.append(result)

            command_name = concat_names(api_object.name, method.name)
            command = CommandType(command_name, {
                'category': 'command',
                'members': members,
            })

            command.derived_object = api_object
            command.derived_method = method
            add_command(command)

        # Create builder return ErrorCallback commands
        if api_object.is_builder:
            command_name = concat_names(api_object.name, 'error callback')
            built_object = RecordMember(Name('built object'), types['ObjectHandle'], 'value', False, False)
            built_object.set_target_type(api_object.built_type)
            command = CommandType(command_name, {
                'category': 'return command',
                'members': [
                    built_object,
                    RecordMember(Name("status"), types['uint32_t'], 'value', False, False),
                    string_message_member,
                ]
            })
            command.derived_object = api_object
            add_command(command)

    for (name, json_data) in wire_json.items():
        if name[0] == '_':
            continue
        command = CommandType(name, {
            'category': json_data['category'],
            'members': linked_record_members(json_data.get('members', []), types),
        })
        add_command(command)

    for commands in wire_params['cmd_records'].values():
        for command in commands:
            command.update_metadata()
        commands.sort(key=lambda c: c.name.canonical_case())

    wire_params.update(wire_json.get('_params', {}))

    return wire_params
