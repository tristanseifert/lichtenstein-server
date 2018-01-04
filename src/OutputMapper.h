/**
 * The output mapper builds a relation between output groups (or a collection of
 * groups, called an ubergroup) and an effect routine.
 */
#ifndef OUTPUTMAPPER_H
#define OUTPUTMAPPER_H

#include "HSIPixel.h"
#include "db/Group.h"

#include <map>
#include <set>
#include <vector>

#include "INIReader.h"

class Routine;

class DataStore;
class Framebuffer;

class OutputMapper {
	public:
		OutputMapper(DataStore *s, Framebuffer *f, INIReader *reader);
		~OutputMapper();

	public:
		class OutputGroup {
			friend class OutputMapper;

			public:
				OutputGroup() = delete;
				OutputGroup(DbGroup *g);
				virtual ~OutputGroup() {};

				/**
				 * Returns an iterator into the group's framebuffer.
				 */
				std::vector<HSIPixel>::iterator getDataPointer() {
					return this->buffer.begin();
				}

				/**
				 * Returns the number of pixels in the group.
				 */
				int numPixels() const {
					return this->group->numPixels();
				}

			private:
				DbGroup *group = nullptr;

				std::vector<HSIPixel> buffer;

				friend bool operator==(const OutputGroup& lhs, const OutputGroup& rhs);
				friend bool operator< (const OutputGroup& lhs, const OutputGroup& rhs);
		};

		class OutputUberGroup: public OutputGroup {
			friend class OutputMapper;

			public:
				OutputUberGroup();
				OutputUberGroup(std::vector<OutputGroup *> &members);
				~OutputUberGroup() {};

			private:
				void addMember(OutputGroup *group);
				void removeMember(OutputGroup *group);
				bool containsMember(OutputGroup *group);

			private:
				void _resizeBuffer();

			private:
				std::set<OutputGroup *> groups;
		};

	public:
		void addMapping(OutputGroup *g, Routine *r);
		void removeMappingForGroup(OutputGroup *g);

		inline Routine *routineForMapping(OutputGroup *g) {
			return this->outputMap[g];
		}

	private:
		void _removeMappingsInUbergroup(OutputUberGroup *ug);

	private:
		DataStore *store;
		Framebuffer *fb;
		INIReader *config;

		std::map<OutputGroup *, Routine *> outputMap;
};

#endif
