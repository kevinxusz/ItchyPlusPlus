#include "scratchio.h"

OSB::OSB(uint8_t* s, size_t n) {
	char* c = (char*) s;
	setg(c, c, c + n);
}


ByteStream::ByteStream(istream* stream) {
	this->stream = stream;
}

ByteStream::ByteStream(uint8_t* block, uint32_t size) {
    this->stream = new std::istream(new OSB(block, size - 1));
}

uint8_t ByteStream::uint8() {
	uint8_t n;
	this->readBlockR(&n, sizeof(uint8_t));
	return n;
}

uint16_t ByteStream::uint16() {
	uint16_t n;
	this->readBlockR(&n, sizeof(uint16_t));
	return n;
}

uint32_t ByteStream::uint32() {
	uint32_t n;
	this->readBlockR(&n, sizeof(uint32_t));
	return n;
}

uint64_t ByteStream::uint64() {
	uint64_t n;
	this->readBlockR(&n, sizeof(uint64_t));
	return n;
}

int8_t ByteStream::int8() {
	int8_t n;
	this->readBlockR(&n, sizeof(int8_t));
	return n;
}

int16_t ByteStream::int16() {
	int16_t n;
	this->readBlockR(&n, sizeof(int16_t));
	return n;
}

int32_t ByteStream::int32() {
	int32_t n;
	this->readBlockR(&n, sizeof(int32_t));
	return n;
}

int64_t ByteStream::int64() {
	int64_t n;
	this->readBlockR(&n, sizeof(int64_t));
	return n;
}

float ByteStream::float32() {
	uint32_t n;
	this->readBlockR(&n, sizeof(float));
	return htobe32(n);
}

double ByteStream::float64() {
	uint64_t n;
	this->readBlockR(&n, sizeof(double));
	return htobe64(n);
}

char* ByteStream::readString(uint32_t size) {
	char* string = new char[size];
	this->readBlock(string, size);
	return string;
}

void ByteStream::readBlock(void* block, uint32_t size) {
	this->stream->read((char*) block, size);
}

void ByteStream::readBlockR(void* block, uint32_t size) {
	this->readBlock(block, size);
	std::reverse((char*) block, (char*) block + size);
}


ObjectRecord::ObjectRecord(uint8_t id, uint8_t version, char* data, uint32_t dataSize, ObjectRecord** fields, uint32_t fieldCount) {
	this->id = id;
	this->version = version;
	this->data = data;
	this->dataSize = dataSize;
	this->fields = fields;
	this->fieldCount = fieldCount;
}

int32_t ObjectRecord::intValue() {
    switch (this->id) {
    case 4:
        return *(int32_t*) this->data;
    case 5:
        return (int32_t) (*(int16_t*) this->data);
    case 8:
        return (int32_t) (*(double*) this->data);
    }
    return 0;
}

uint32_t ObjectRecord::uintValue() {
    switch (this->id) {
    case 4:
        return *(uint32_t*) this->data;
    case 5:
        return (uint32_t) (*(uint16_t*) this->data);
    case 8:
        return (uint32_t) (*(double*) this->data);
    }
    return 0;
}

double ObjectRecord::doubleValue() {
    switch (this->id) {
    case 4:
        return (double) (*(int64_t*) this->data);
    case 5:
        return (double) (*(int16_t*) this->data);
    case 8:
        return *(double*) this->data;
    }
    return 0;
}

uint32_t ObjectRecord::colorValue() {
    if (this->id == 30 || this->id == 31) {
        return *(uint32_t*) this->data;
    }
    return 0;
}

bool ObjectRecord::isNull() {
    return this->id == 1;
}


ScratchReader::ScratchReader(ByteStream* stream) {
	this->stream = stream;
}

ScratchReader::~ScratchReader() {

}


Stage* ScratchReader::readProject() {
	if (strcmp(this->stream->readString(10), "ScratchV02") == 0) {
		cout << "Scratch Project" << endl;

		cout << "Info Size: " << this->stream->uint32() << endl;

		ObjectRecord* infoRecord = this->readObjectStore();
		ObjectRecord* stageRecord = this->readObjectStore();

		Stage* stage = new Stage(stageRecord);
		return stage;
	}
	cout << "Not a Scratch Project" << endl;
	return NULL;
}

ObjectRecord* ScratchReader::readObjectStore() {
	if (memcmp(this->stream->readString(10), "ObjS\x01Stch\x01", 10) == 0) {
		cout << "Is Object" << endl;

		uint32_t size = this->stream->uint32();

		cout << "Object Size: " << size << endl;
		ObjectRecord** table = new ObjectRecord*[size];

		for (uint32_t i = 0; i < size; i++) {
			table[i] = this->readObject();
		}

		for (uint32_t i = 0; i < size; i++) {
			ObjectRecord* object = table[i];
			ObjectRecord** fields = object->fields;
			if (fields != NULL) {
				for (uint32_t j = 0; j < object->fieldCount; j++) {
					if (fields[j]->id == 99) {
						uint32_t pointer = *(uint32_t*) fields[j]->data;
						delete fields[j];
						fields[j] = table[pointer - 1];
					}
				}
			}
		}

		return table[0];
	}
	cout << "Not Object" << endl;
	return NULL;
}

ObjectRecord* ScratchReader::readObject() {
	uint8_t id = this->stream->uint8();
	//cout << "id: " << (int) id << endl;
	if (id < 100) {
		return readFixedFormat(id);
	}

	uint8_t version = this->stream->uint8();

	uint8_t fieldCount = this->stream->uint8();
	ObjectRecord** fields = new ObjectRecord*[fieldCount];

	for (uint32_t i = 0; i < fieldCount; i++) {
		fields[i] = this->readObject();
	}

	return new ObjectRecord(id, version, NULL, 0, fields, fieldCount);
}

ObjectRecord* ScratchReader::readFixedFormat(uint8_t id) {
	uint32_t length = 0;

	ObjectRecord** fields = NULL;
	uint32_t fieldCount = 0;

	char* data = NULL;
	uint32_t color;

	switch (id) {
	case 1: // Nil
	case 2: // True
	case 3: // False
		length = 0;
		break;
	case 4: // SmallInteger
		length = 4;
		data = new char[length];
		this->stream->readBlockR(data, length);
		break;
	case 5: // SmallInteger16
		length = 2;
		data = new char[length];
		this->stream->readBlockR(data, length);
		break;
	case 6: //LargePositiveInteger
		//Not sure if it is 6.
		length = 6;
		data = new char[length];
		this->stream->readBlockR(data, length);
		break;
	case 7: //LargeNegativeInteger
		//Not sure if it is 7.
		length = 7;
		data = new char[length];
		this->stream->readBlockR(data, length);
		break;
	// long int stuff to come
	case 8: // Float
		length = 8;
		data = new char[length];
		this->stream->readBlockR(data, length);
		break;
	case 9: // String
	case 10: // Symbol
	case 11: // ByteArray
	case 14: // UTF8
		length = this->stream->uint32();
		break;
	case 12: // SoundBuffer
		length = this->stream->uint32() * 2;
		break;
	case 13: // Bitmap
		length = this->stream->uint32() * 4;
		break;
	case 20: // Array
	case 21: // OrderedCollection
	case 24: // Dictionary
	case 25: // IdentityDictionar
	case 32: // Point
	case 33: // Rectangle
	case 34: // Form
	case 35: // ColorForm
		switch (id) {
			case 20:
			case 21:
				length = this->stream->uint32();
				break;
			case 24:
			case 25:
				length = this->stream->uint32() * 2;
				break;
			case 32:
				length = 2;
				break;
			case 33:
				length = 4;
				break;
			case 34:
				length = 5;
				break;
			case 35:
				length = 6;
				break;
		}
		fields = new ObjectRecord*[length];
		fieldCount = length;

		for (uint32_t i = 0; i < length; i++) {
			fields[i] = this->readObject();
		}
		length = 0;
		break;
	case 30: // Color
		length = 4;
		data = new char[length];
		this->stream->readBlockR(data, length);
		color = *(uint32_t*) data;
		data[0] = color >> 2 & 0xFF;
		data[1] = color >> 12 & 0xFF;
		data[2] = color >> 22 & 0xFF;
		data[3] = 0xff;
		break;
	case 31: // TranslucentColor
		length = 4;
		data = new char[4];
		this->stream->readBlockR(data, 4);
		color = *(uint32_t*) data;
		data[0] = color >> 2 & 0xFF;
		data[1] = color >> 12 & 0xFF;
		data[2] = color >> 22 & 0xFF;
		data[3] = this->stream->uint8();
		break;
	case 99: // ObjectRef
		length = 4;
		data = new char[length];
		data[3] = 0;
		this->stream->readBlockR(data, 3);
		break;
	default:
		cout << "Unknown field ID: " << (int) id << endl;
	}

	if (data == NULL) {
		data = new char[length];
		this->stream->readBlock(data, length);
	}

	ObjectRecord* record = new ObjectRecord(id, 0, data, length, fields, fieldCount);

	return record;
}


Stage* openFromFile(const char* path) {
	ifstream::pos_type size;

    ifstream file(path, ios::in|ios::binary);

    if (file.is_open()) {
		Stage* stage = openFromStream(&file);

        file.close();

        return stage;
    }
	cout << "Unable to open file: " << path << endl;
	exit(0);
	return NULL;
}

Stage* openFromStream(istream* s) {
	ByteStream stream(s);
	ScratchReader reader(&stream);
	return reader.readProject();
}
